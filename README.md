# T3 Pop

A small popup for sending a message, with screenshots, straight to a
project's orchestrator thread in T3 Code. You then carry on the conversation
in T3 Code, which renders the reply.

The target is the project's thread titled exactly `Orchestrator`. If a
project has none, the first message starts one with that title.

## Projects

`~/.config/t3pop/projects.json` lists the projects, first one default:

```json
[
  { "name": "Hub", "path": "~/Hub" },
  { "name": "T3 Code", "path": "~/source/t3code" }
]
```

Each path must be a T3 Code project (`t3 project add <path>`).

## Screenshots

Paste an image with Ctrl+V or grab a region with Ctrl+Shift+S (Spectacle).
Images are saved under `~/.cache/t3pop` and pruned after 14 days.

When the installed `t3` CLI supports `thread send --image`, screenshots go as
native attachments. Older CLIs get the file paths in the message text, and
the agent opens them itself.

## Shortcuts

| Key | Action |
| --- | --- |
| Enter | Send to the orchestrator |
| Shift+Enter | New line |
| Ctrl+V | Paste text or an image |
| Ctrl+Shift+S | Screenshot a region |
| Ctrl+1 to 9 | Choose the project |
| Esc | Put away (a draft is kept; an empty popup quits) |
| Ctrl+Q | Quit |
| F1 | Show or hide shortcuts |

Launching it again while it is open brings the existing popup back.

## Command line

```sh
t3pop projects [--json]
t3pop send [--project NAME] [--image FILE]... MESSAGE [--json]
```

## Build

```sh
bin/build      # build/t3pop
bin/test       # backend tests, offscreen
bin/install    # into ~/.local, shows up in the launcher
bin/uninstall
```
