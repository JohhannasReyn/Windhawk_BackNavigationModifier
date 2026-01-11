# Smart Explorer Navigation

Fixes annoying Windows Explorer navigation behavior:

## Features
- **Smart Back Navigation**: When back/backspace fails, automatically navigates to parent directory
- **Fallback Chain**: Back → Parent → This PC → Desktop
- **Silent Failures**: No error dings when navigation/paths fail
- **Bonus Hotkey**: Shift+Backspace for "up" navigation (configurable)
- **Intelligent Swapping**: If "up" fails, tries "back" instead

## Why This Exists
Windows Explorer loves to ding at you when you hit backspace in the wrong context,
alerting you with the, "Idio-ding!". This mod makes navigation
more forgiving and QUIET - so Windows, can STFU, cuz we don't need everyone knowing.

## Usage
- Backspace/Back button: Smart back with fallback
- Shift+Backspace (or Alt+Backspace): Navigate up with fallback
- Failed path navigation: Silent failure (no action, no ding)

Customize the hotkey in settings!

---
**'WindhawkModSettings'**

### Configure the additional navigation hotkey, aside from:
- 'Alt'+<Arrow Keys: i.e. Left='Back', Up='UP', Right='Forward'> 

### An additional hotkey can be implemented (configurable): 
- 'Shift'+'Backspace'
- -or-
- 'Alt'+'Backspace'.

### ***The Complete Patch Navigation Flow Then Becomes***
- If either 'Back' or 'Up' is unavailable, then the other is used.
- If both are unavailable, then Explorer navigates to the next available parent (Up) directory.
- If the entire path is corrupt (i.e. a removable drive is hot-pulled) then Explorer navigates to 'This PC'. 
- If a literal navigation to a path doesn't exist, Windows should STFU and fail quietly!
- That is all 😊
