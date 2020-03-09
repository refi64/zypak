# zypak

Allows you to run Electron binaries that require a sandbox in a Flatpak environment,
by using LD_PRELOAD magic and a redirected sandbox.

## Basic usage

This requires your Flatpak to be using:

- `org.freedesktop.Platform` / `Sdk` version `19.08`.
- `org.electronjs.Electron2.BaseApp` as your base app. Recent releases include Zypak
  built-in.

Now, instead of running your Electron binary directly, call it via
`zypak-wrapper PATH/TO/MY/ELECTRON/BINARY`.

## Common problems

If there is no `chrome-sandbox` binary in the Electron app's folder (e.g. it
was removed so that the namespace sandbox would be exclusively used), you need
to create a stub one. Just `touch chrome-sandbox && chmod +x chrome-sandbox`,
and everything should work.

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

## Debugging

- Set `ZYPAK_DEBUG=1` or pass `-d` to `zypak-wrapper` to enable debug logging.
- Set `ZYPAK_DISABLE_SANDBOX=1` to disable the use of the `--sandbox` argument
  (required if the Electron binary is not installed, as the sandboxed calls will be unable to locate the Electron binary).

## Random notes

This has been successfully lightly tested with Chromium itself.
