# HLauncher

A cross-platform Minecraft launcher starter in modern C++.

## Build

```bash
/Applications/CMake.app/Contents/bin/cmake --preset default
/Applications/CMake.app/Contents/bin/cmake --build --preset default
```

## Run

Launch the app window after building:

```bash
export HLAUNCHER_TENANT_ID="common"
export HLAUNCHER_REDIRECT_URI="hl://auth"
export HLAUNCHER_SCOPE="openid offline_access XboxLive.signin"
./build/default/bin/HLauncher.app/Contents/MacOS/HLauncher
```

On macOS, launch the `.app` bundle so LaunchServices can register the `hl` scheme and the GUI window can receive the redirect. On Windows and Linux, use the platform-specific executable under the preset's `bin` directory.

## Custom URI registration

The launcher expects the OS to route `hl://auth` back to the executable.

On macOS, you must build and launch the `.app` bundle so LaunchServices can register the `hl` scheme. Running the raw executable directly will not register the URL handler, which is why Safari shows "address is invalid".

This project does not currently use MSAL or the Apple auth broker flow from the iOS/macOS quickstart. The quickstart's `msauth.<bundle-id>://auth` redirect, `LSApplicationQueriesSchemes`, and `AppDelegate`/`SceneDelegate` hooks apply only if you convert this launcher into a native Apple app bundle and adopt MSAL.

### Windows

Register the `hl` scheme in `HKEY_CURRENT_USER\Software\Classes\hl` or `HKEY_CLASSES_ROOT` and point `shell\open\command` to the launcher executable with `%1`.

### macOS

The CMake build now generates a macOS app bundle with `CFBundleURLTypes` for `hl://auth`. Launch the bundle at least once so macOS registers it as the handler for the `hl` scheme.

### Linux

Install a `.desktop` file with `MimeType=x-scheme-handler/hl;` and register it with `xdg-mime default`.

## Current behavior

The starter project already:

- builds the authorization URL with PKCE
- opens the browser on the system default handler
- captures `hl://...` callbacks through single-instance forwarding
- parses the returned authorization code and state
- shows a native cross-platform launcher window with sign-in status

Token exchange is intentionally left as the next step.
