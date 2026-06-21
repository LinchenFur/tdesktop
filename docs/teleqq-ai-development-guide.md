# TeleQQ AI Development Guide

This fork keeps Telegram Desktop as the UI shell and replaces the Telegram
network/data path with a NapCat OneBot WebSocket adapter. Do not rewrite the UI
with web or custom Qt widgets. The target product is a second QQ client rendered
inside Telegram Desktop's native chat list, history view, composer, avatars, and
typing indicators.

## Current Scope

- Frontend: original Telegram Desktop UI.
- Backend: NapCat WebSocket OneBot API.
- Main branch for this work: `teleqq-napcat`.
- Windows builds are produced by GitHub Actions. Avoid local full builds unless
  the user explicitly asks for them.

## Important Files

- `Telegram/SourceFiles/teleqq/teleqq_types.h`
  Shared TeleQQ domain types: `Chat`, `Message`, `Attachment`, API result types.
- `Telegram/SourceFiles/teleqq/teleqq_onebot.cpp`
  Converts OneBot/NapCat JSON into TeleQQ chats and messages. This is where
  segment rendering, attachment extraction, QQ avatar URLs, and send parameters
  belong.
- `Telegram/SourceFiles/teleqq/teleqq_napcat_client.cpp`
  WebSocket RPC client. Owns request/response echo matching, list/history
  requests, live event handling, `get_file` resolution, and reconnection.
- `Telegram/SourceFiles/teleqq/teleqq_store.cpp`
  Small in-memory store for NapCat chats/messages before projection into
  Telegram's data model.
- `Telegram/SourceFiles/core/application.cpp`
  Projects TeleQQ chats/messages/events into Telegram native `Data::Session`.
  This is where native peers, dialogs, history messages, avatars, media, and
  typing updates are injected.
- `Telegram/SourceFiles/apiwrap.cpp`
  Hooks Telegram API calls. In TeleQQ mode, history and send-message requests
  are redirected to NapCat instead of Telegram servers.
- `.github/workflows/teleqq-win.yml`
  Windows CI build used to produce downloadable artifacts.

## Data Flow

Startup/login:

1. User enters NapCat WebSocket endpoint and optional token in the Telegram
   login shell.
2. `Core::Application` creates `TeleQQ::NapcatClient`.
3. Friend and group lists are requested from NapCat.
4. `ProjectTeleqqChatToNativeList()` creates Telegram native users/chats and
   dialogs so the original Telegram list UI renders QQ contacts/groups.

Live message:

1. NapCat pushes a OneBot `message` or `message_sent` event.
2. `NapcatClient::handleTextFrame()` converts it with
   `OneBot::MessageFromEvent()`.
3. `NapcatClient::resolveMessageFiles()` calls NapCat `get_file` for
   attachments that only contain `file_id`.
4. `Store::addMessage()` stores and emits the TeleQQ message.
5. `ProjectTeleqqMessageToNativeHistory()` creates a native Telegram
   `HistoryItem`.

History:

1. Telegram history view calls `ApiWrap::requestHistory()`.
2. TeleQQ mode redirects this to NapCat `get_group_msg_history` or
   `get_friend_msg_history`.
3. Returned OneBot messages are parsed, file IDs are resolved, projected into
   native Telegram messages, and then returned to Telegram via
   `history->messages().addSlice(...)`.

Sending:

1. Telegram composer calls `ApiWrap::sendMessage()`.
2. TeleQQ mode sends `send_group_msg` or `send_private_msg`.
3. The response message ID is used to create the local outgoing message.

## Mapping Rules

- Private QQ user peer: Telegram `UserId(TeleqqBareId(user_id))`.
- QQ group peer: Telegram `ChatId(TeleqqBareId(group_id))`.
- QQ group sender: native Telegram user created from `authorId`.
- QQ user avatar URL:
  `https://q.qlogo.cn/headimg_dl?dst_uin=<uin>&spec=640&img_type=jpg`
- QQ group avatar URL:
  `https://p.qlogo.cn/gh/<group>/<group>/640`
- Image/file segments with HTTP URLs become Telegram `messageMediaDocument`.
- File/image segments with only `file_id` must be resolved through NapCat
  `get_file` before projection.
- NapCat input status notices should be converted to
  `MTP_updateUserTyping` or `MTP_updateChatUserTyping`.

## Known Pitfalls

- Do not add a replacement HTML/Vite UI for the product. The requirement is
  Telegram Desktop UI with NapCat data.
- Adding native messages is not enough for history. The active history request
  also needs `history->messages().addSlice(...)` with projected `MsgId`s.
- Group messages must set `from_id` to the real QQ sender user, not the group
  peer, otherwise every group message appears to come from one person.
- Images displayed as `:-(` usually mean Telegram received a document without a
  usable thumbnail/web location. Keep the URL in `ImageLocation`.
- If a file segment has no URL, keep its textual placeholder until `get_file`
  resolves it. Do not strip `[文件 name]` unless native media exists.
- Avoid changing unrelated upstream Telegram code. Keep TeleQQ changes scoped to
  the adapter/projection points above.

## Build Workflow

Use GitHub Actions for Windows artifacts:

```powershell
gh workflow run teleqq-win.yml -R LinchenFur/tdesktop --ref teleqq-napcat
gh run list -R LinchenFur/tdesktop --branch teleqq-napcat --limit 5
gh run view <run-id> -R LinchenFur/tdesktop --json status,conclusion,jobs,url
gh run download <run-id> -R LinchenFur/tdesktop --dir D:\GitHub\TeleQQ\github-artifacts\<run-id>
```

Local launcher path:

```text
D:\GitHub\TeleQQ\run-teleqq.cmd
```

If CI is slow, prefer improving `.github/workflows/teleqq-win.yml` cache keys and
build output caching. Do not switch to local compilation unless requested.
