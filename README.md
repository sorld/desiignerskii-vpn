# Desiignerskii VPN

Клиент для Windows 10 и 11 x64 на базе Throne 1.4.2. В одном окне можно выбрать сервер из подписки, посмотреть задержку и включить VPN. По умолчанию клиент использует TUN. В настройках доступны системный прокси, DNS и правила маршрутизации для сайтов и приложений.

## Подключение

1. Распакуй архив для Windows и запусти `Desiignerskii VPN.exe`. Для интерфейса нужен Microsoft Edge WebView2 Runtime.
2. Нажми **Добавить устройство** и вставь персональную ссылку из своего кабинета.
3. Выбери сервер в списке и нажми **Подключить**. При первом включении TUN Windows может запросить разрешение для сетевого движка.

## Сборка из исходников

На Windows x64 с Go 1.27.4 выполни в PowerShell из корня репозитория

```powershell
cd desktop/desiignerskii
go test ./...
.\build-windows.ps1
```

Скрипт проверит SHA-256 официального архива движка. Готовая программа появится в папке `desktop/desiignerskii/build`.

[Руководство клиента](desktop/desiignerskii/README.ru.md) · [Документация Throne](README.upstream.md) · [Исходный проект Throne](https://github.com/throneproj/Throne) · [Лицензия](LICENSE)
