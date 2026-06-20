# TeleQQ tdesktop adapter

This directory is the direct tdesktop-side NapCat / OneBot v11 adapter.

Current scope:

- `teleqq_websocket.*`: minimal RFC 6455 client over `QTcpSocket`, targeting local `ws://` NapCat endpoints without adding a Qt WebSockets dependency.
- `teleqq_napcat_client.*`: OneBot action/event transport, echo response matching, reconnect, and optional environment-based configuration.
- `teleqq_onebot.*`: conversion helpers from OneBot JSON to a small TeleQQ model.
- `teleqq_store.*`: in-memory chat/message store populated from NapCat roster and message events.
- `teleqq_panel.*`: a minimal in-process Qt widget embedded in Telegram Desktop's main window for reading chats, reading messages, and sending text through NapCat.

Runtime configuration:

The app now starts in TeleQQ / NapCat mode instead of the native Telegram
account/login flow. Telegram Desktop's native main window is created first,
then `MainWindow::setupTeleqq()` replaces the login surface with the embedded
NapCat connection/chat surface. Configure the forward WebSocket endpoint and
optional token in the top bar, then connect.

Environment variables are still accepted as startup defaults and auto-connect
when present:

```powershell
$env:TELEQQ_NAPCAT_WS = "ws://127.0.0.1:3001/"
$env:TELEQQ_NAPCAT_TOKEN = "your_secret_token"
```

On connect it requests `get_friend_list` and `get_group_list`, stores incoming
message events in `TeleQQ::Store`, and keeps the TeleQQ surface inside the
primary Telegram window.

Next integration step is to replace the minimal panel with a native dialogs/history surface, using `TeleQQ::Store::chats()` and `TeleQQ::Store::messages(chatId)` as the data source.
