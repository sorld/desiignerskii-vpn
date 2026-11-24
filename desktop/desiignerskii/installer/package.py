"""Build a Windows x64 installer from the unpacked desktop release and Microsoft's WebView2 bootstrapper."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import uuid
import xml.etree.ElementTree as ET

NAMESPACE = uuid.UUID("a7a06271-f61c-4df3-8de8-e8869fcccb8d")
UPGRADE_CODE = "{F721C426-31F1-4E41-B9A4-FF8088CFCFAB}"
APP = "Desiignerskii VPN"
HERE = Path(__file__).resolve().parent

def guid(value):
    return "{" + str(uuid.uuid5(NAMESPACE, value)).upper() + "}"

def ident(prefix, value):
    return prefix + hashlib.sha256(value.encode()).hexdigest()[:26]

def element(parent, name, **attrs):
    return ET.SubElement(parent, name, attrs)

def build_msi(stage, version):
    product_code = guid("product/x64/" + version)
    wix = ET.Element("Wix", xmlns="http://schemas.microsoft.com/wix/2006/wi")
    product = element(wix, "Product", Id=product_code, Name=APP, Version=version, Codepage="1251",
                      Manufacturer="Desiignerskii", Language="1049", UpgradeCode=UPGRADE_CODE)
    element(product, "Package", InstallerVersion="500", Compressed="yes", InstallScope="perMachine")
    element(product, "Media", Id="1", Cabinet="client.cab", EmbedCab="yes")
    element(product, "Property", Id="ARPPRODUCTICON", Value="AppIcon")
    element(product, "Property", Id="ARPURLINFOABOUT", Value="https://github.com/sorld/desiignerskii-vpn")
    element(product, "Icon", Id="AppIcon", SourceFile="icon.ico")
    element(product, "MajorUpgrade", DowngradeErrorMessage="A newer version of Desiignerskii VPN is already installed.")
    root = element(product, "Directory", Id="TARGETDIR", Name="SourceDir")
    program_files = element(root, "Directory", Id="ProgramFiles64Folder")
    install_dir = element(program_files, "Directory", Id="INSTALLDIR", Name=APP)
    element(root, "Directory", Id="ProgramMenuFolder")
    element(root, "Directory", Id="DesktopFolder")
    feature = element(product, "Feature", Id="Client", Title=APP, Level="1")
    payload = stage / "payload"
    folders = {Path("."): install_dir}
    for path in sorted(payload.rglob("*")):
        rel = path.relative_to(payload)
        key = rel.as_posix()
        parent = folders[rel.parent]
        if path.is_dir():
            folders[rel] = element(parent, "Directory", Id=ident("D", key), Name=rel.name)
            continue
        component_id = ident("C", key)
        target = element(parent, "Component", Id=component_id, Guid=guid("file/" + key), Win64="yes")
        file_id = "EngineExe" if key == "engine/Throne.exe" else ident("F", key)
        element(target, "File", Id=file_id, Name=rel.name, Source="payload/" + key, KeyPath="yes")
        if key == APP + ".exe":
            for shortcut, folder in (("StartMenuShortcut", "ProgramMenuFolder"), ("DesktopShortcut", "DesktopFolder")):
                element(target, "Shortcut", Id=shortcut, Name=APP, Directory=folder, Target=f"[#{file_id}]", WorkingDirectory="INSTALLDIR", Icon="AppIcon", IconIndex="0")
        element(feature, "ComponentRef", Id=component_id)
    # Invoke the same per-user grant cleanup as Throne's uninstaller on explicit removal.
    element(product, "CustomAction", Id="ReleaseTunGrant", FileKey="EngineExe", ExeCommand="--elevated-helper uninstall", Execute="immediate", Return="check")
    seq = element(product, "InstallExecuteSequence")
    element(seq, "Custom", Action="ReleaseTunGrant", Before="RemoveFiles").text = 'Installed AND REMOVE="ALL" AND NOT UPGRADINGPRODUCTCODE'
    ET.indent(wix)
    ET.ElementTree(wix).write(stage / "client.wxs", encoding="utf-8", xml_declaration=True)
    subprocess.run(["wixl", "-a", "x64", "-o", "client.msi", "client.wxs"], cwd=stage, check=True)
    # wixl schedules major upgrades before the transaction by default.
    subprocess.run(["msibuild", "client.msi", "-q", "UPDATE `InstallExecuteSequence` SET `Sequence`=1501 WHERE `Action`='RemoveExistingProducts'"], cwd=stage, check=True)
    return product_code

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("icon", type=Path)
    parser.add_argument("bootstrapper", type=Path)
    parser.add_argument("version")
    parser.add_argument("outdir", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version):
        parser.error("version must contain three numbers")
    major, minor, build = map(int, args.version.split("."))
    if major > 255 or minor > 255 or build > 65535:
        parser.error("version exceeds Windows Installer limits")
    for rel in (APP + ".exe", "engine/Throne.exe", "engine/ThroneCore.exe", "engine/libcronet.dll"):
        if not (args.payload / rel).is_file():
            parser.error(f"missing payload file {rel}")
    args.outdir.mkdir(parents=True, exist_ok=True)
    output = args.outdir.resolve() / f"Desiignerskii-VPN-{args.version}-Setup-x64.exe"
    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp)
        shutil.copytree(args.payload, stage / "payload")
        for src, dest in [(args.icon,"icon.ico"), (args.bootstrapper,"webview.exe"), (HERE/"wizard.bmp","wizard.bmp"), (HERE/"setup.nsi","setup.nsi")]:
            shutil.copyfile(src, stage / dest)
        product_code = build_msi(stage, args.version)
        subprocess.run(["makensis", "-V3", "-DVERSION="+args.version, "-DPRODUCT_CODE="+product_code, "-DOUTPUT="+output.name, "setup.nsi"], cwd=stage, check=True)
        shutil.copyfile(stage / output.name, output)
    print(output)

if __name__ == "__main__":
    main()
