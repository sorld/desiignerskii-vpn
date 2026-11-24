# Desiignerskii VPN

Клиент для Windows 10 и 11 x64 на базе Throne 1.4.2. В одном окне можно выбрать сервер из подписки, посмотреть задержку и включить VPN. В настройках доступны TUN, системный прокси, DNS и правила маршрутизации для сайтов и приложений.

## Нативный клиент для Windows

[Скачать установщик 0.6.0 для Windows x64](https://github.com/sorld/desiignerskii-vpn/releases/download/native-v0.6.0/Desiignerskii-VPN-0.6.0-Setup-x64.exe)

В версии 0.6.0 интерфейс встроен в Throne.exe. На панели со списком серверов нажми **Добавить подписку**, вставь HTTP(S)-ссылку своего VPN-провайдера и подтверди добавление. Можно задать название и выбрать автоматическое обновление. Каждая подписка получает свою вкладку. В меню вкладки можно обновить только выбранную группу.

Установщик использует папку существующего Throne или FSNT. Для новой установки текущему пользователю предлагается `%LOCALAPPDATA%\Throne`. Клиент открывает профили из конфигурации Throne. У версии 0.5.1 отдельное хранилище.

Для сборки x64 под Windows запусти `script/build-native-x64.ps1` в x64 Developer PowerShell для Visual Studio с CMake, Ninja, 7-Zip и Inno Setup 6. Установщик и переносной ZIP появятся в папке `deployment`.

## Установка предыдущей версии 0.5.1

1. [Скачай установщик для Windows x64](https://github.com/sorld/desiignerskii-vpn/releases/download/desktop-v0.5.1/Desiignerskii-VPN-0.5.1-Setup-x64.exe), запусти его и открой **Desiignerskii VPN** через меню «Пуск». Мастер установит WebView2, если потребуется.
2. Нажми **Добавить устройство** и вставь персональную ссылку из своего кабинета.
3. Выбери сервер в списке и нажми **Подключить**. При первом включении TUN Windows может запросить разрешение для сетевого движка.

Для переносного запуска скачай архив **windows-x64.zip** со [страницы релиза](https://github.com/sorld/desiignerskii-vpn/releases/latest) и распакуй его целиком.

## Сборка версии 0.5.1 из исходников

На Windows x64 с Go 1.27.4 выполни в PowerShell из корня репозитория

```powershell
cd desktop/desiignerskii
go test ./...
.\build-windows.ps1
```

Скрипт проверит SHA-256 официального архива движка. Готовая программа появится в папке `desktop/desiignerskii/build`.

[Сборка установщика](desktop/desiignerskii/installer/README.ru.md) · [Руководство клиента](desktop/desiignerskii/README.ru.md) · [Документация Throne](README.upstream.md) · [Исходный проект Throne](https://github.com/throneproj/Throne) · [Лицензия](LICENSE)
