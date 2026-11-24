# Установщик для Windows x64

Мастер использует Windows Installer для установки в Program Files, создания ярлыков, обновления и удаления клиента. Настройки и подписки хранятся отдельно в профиле пользователя. Для тихой установки передай Setup.exe параметр `/S`.

## Сборка

Потребуются Python 3.9+, wixl, msitools и NSIS 3 с поддержкой amd64-unicode. Распакуй архив windows-x64.zip нужного релиза в каталог payload. Скачай официальный [Microsoft Edge WebView2 Evergreen Bootstrapper](https://go.microsoft.com/fwlink/p/?LinkId=2124703) как webview-bootstrap.exe и проверь подпись Microsoft Corporation.

Из корня репозитория выполни команду, указав версию и пути к входным файлам.

```sh
python3 desktop/desiignerskii/installer/package.py \
  'payload/Desiignerskii VPN' desktop/desiignerskii/icon.ico \
  webview-bootstrap.exe 0.5.1 dist
```

Готовый файл появится в dist. Загружай его в релиз с той же версией клиента.
