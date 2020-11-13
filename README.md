# zypak

Allows you to run Electron binaries that require a sandbox in a Flatpak environment,
by using LD_PRELOAD magic and a redirected sandbox.

## Basic usage

This requires your Flatpak to be using:

- `org.freedesktop.Platform` / `Sdk` version `19.08` or later.
- `org.electronjs.Electron2.BaseApp` as your base app. Recent releases include Zypak
  built-in.

Now, instead of running your Electron binary directly, call it via
`zypak-wrapper PATH/TO/MY/ELECTRON/BINARY`.

## Common problems

If there is no `chrome-sandbox` binary in the Electron app's folder (e.g. it
was removed so that the namespace sandbox would be exclusively used), you need
to create a stub one. Just `touch chrome-sandbox && chmod +x chrome-sandbox`,
and everything should work.

### Usage with a wrapper script

If this is wrapping an application that requries some sort of wrapper script,
make sure you set `CHROME_WRAPPER=` to the path of said script. Otherwise, if the
application attempts to re-exec itself (i.e. `chrome://restart`), it won't be using
the wrapper on re-exec, leading to potentially unexpected behavior.

## Using a different version

If you want to try a different Zypak version for testing, or without using the
Electron baseapp, then find the release tag you want to use and add one of these
modules somewhere in your Flatpak manifest:

```json
{
  "name": "zypak",
  "sources": [
    {
      "type": "git",
      "url": "https://github.com/refi64/zypak",
      "tag": "THE_RELEASE_TAG"
    }
  ]
}
```

```yaml
- name: zypak
  sources:
    - type: git
      url: https://github.com/refi64/zypak
      tag: THE_RELEASE_TAG
```

## Experimental functionality

### Forcing use of the file chooser portal

For Electron apps, you can run them with `ZYPAK_FORCE_FILE_PORTAL=1` to force the use of the file
chooser portal over the custom GTK-drawn dialogs. This comes with some very notable trade-offs:

- Selecting directories does not work on any systems with xdg-desktop-portal < 1.5.2.
- The methods for overriding this are very hacky and will likely not work in all cases.
  - In particular, any apps that use a transient file chooser will *not* work.

For basic file dialog use, these may be fine.

## Debugging

- Set `ZYPAK_DEBUG=1` to enable debug logging.
- Set `ZYPAK_STRACE=all` to run strace on the host and child processes.
  - To make it host-only or child-only, set `ZYPAK_STRACE=host` or `ZYPAK_STRACE=child`, respectively.
  - Set `ZYPAK_STRACE_FILTER=expr` to pass a filter expression to `strace -e`.
  - In order to avoid arguments being ellipsized, set `ZYPAK_STRACE_NO_LINE_LIMIT=1`.
- Set `ZYPAK_DISABLE_SANDBOX=1` to disable the use of the `--sandbox` argument
  (required if the Electron binary is not installed, as the sandboxed calls will be unable to locate the Electron binary).

## Random notes

This has been successfully lightly tested with Chromium itself.
