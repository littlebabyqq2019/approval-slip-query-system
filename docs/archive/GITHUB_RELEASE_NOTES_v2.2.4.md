# CrossNetShare v2.2.4 - Silent Startup

## 🎉 Overview

v2.2.4 is a stability and user experience release, including critical bug fixes from v2.2.2 to v2.2.4.

## ✨ What's New

### v2.2.4: Silent Startup (Current Release)
- 🔇 **Auto-minimize to system tray** on startup
- 🚫 **Removed all tray notifications** for silent operation
- ⚡ **Improved user experience** for auto-start scenarios
- 💼 **Perfect for unattended operation** in office and server environments

### v2.2.3: Connection Reliability
- 🔄 **Fixed false connection state** after system sleep/wake
- ⏱️ **Improved heartbeat mechanism** with faster detection (30-45s)
- 🔌 **Initial heartbeat verification** after reconnection
- 💚 **Better connection recovery** for long-running scenarios

### v2.2.2: Critical Crash Fix
- 🐛 **Fixed client crash** after indexing completion
- 🔒 **Thread-safe signal handling** using Qt::QueuedConnection
- ✅ **Stable operation** for background indexing

## 🔧 Key Improvements

### Stability
- ✅ No more crashes on startup
- ✅ Thread-safe signal delivery
- ✅ Reliable background indexing

### Connection Reliability
- ✅ 30-45s connection problem detection (down from 60s)
- ✅ Auto-recovery after system sleep/wake
- ✅ Enhanced heartbeat mechanism

### User Experience
- ✅ Silent startup with auto-minimize
- ✅ No notification interruptions
- ✅ Transparent background operation
- ✅ Perfect for auto-start on boot

## 📦 Installation

### Upgrade from Previous Versions

Simply replace the executables:
- `CrossNetShareClient.exe`
- `CrossNetShareServer.exe`

**Fully backward compatible** - no configuration changes needed!

### First-time Installation

1. Download the release files
2. Extract to your desired location
3. Run `CrossNetShareServer.exe` on the server
4. Run `CrossNetShareClient.exe` on each client
5. Configure server address and share path
6. Enable auto-start for silent operation

## 🎯 Usage

### Accessing the Application

After startup, the program minimizes to system tray:
- **Double-click** tray icon to open main window
- **Right-click** tray icon for menu
- **Tray tooltip** shows program name

### System Tray Icons
- 🖧 **Client**: Network drive icon
- 💻 **Server**: Computer icon

## 📝 Technical Details

### Bug Fixes

**v2.2.2 - Critical Crash Fix**:
```cpp
// Added Qt::QueuedConnection for thread safety
connect(indexer_, &FileIndexer::indexingFinished, this, [this]() {
    onLogMessage("Content indexing finished");
}, Qt::QueuedConnection);
```

**v2.2.3 - Heartbeat Improvements**:
```cpp
// Reduced check interval from 60s to 30s
heartbeatCheckTimer_->setInterval(30000);

// Reduced timeout from 60s to 45s
if (secondsSinceLastSent > 45) {
    // Force reconnect
}

// Added initial heartbeat after connection
QTimer::singleShot(1000, this, &Client::sendHeartbeat);
```

**v2.2.4 - Silent Startup**:
```cpp
// Auto-hide without notifications
QTimer::singleShot(1000, this, [this]() {
    hide();
    // No notification messages
});
```

### Files Changed
- `client/ui/main_window.cpp`
- `server/ui/main_window.cpp`
- `client/client.cpp`
- `CMakeLists.txt`

## 📊 Changelog

### v2.2.4 (2026-09-04)
- Feature: Silent startup with auto-minimize
- Improvement: Removed all tray notifications

### v2.2.3 (2026-09-04)
- Fix: False connection state after system sleep
- Improvement: Enhanced heartbeat mechanism
- Improvement: Faster connection problem detection

### v2.2.2 (2026-09-03)
- Fix: Client crash after indexing completion
- Improvement: Thread-safe signal handling

## 🐛 Known Issues

No critical or blocking issues known.

## 📖 Documentation

- [CHANGELOG_v2.2.4.md](CHANGELOG_v2.2.4.md) - Detailed changelog
- [SILENT_STARTUP_FEATURE.md](SILENT_STARTUP_FEATURE.md) - Silent startup feature guide
- [SLEEP_RECONNECT_FIX.md](SLEEP_RECONNECT_FIX.md) - Sleep/wake connection fix
- [CRASH_FIX_v2.2.2.md](CRASH_FIX_v2.2.2.md) - Crash fix technical details

## 🔜 Roadmap

Future improvements may include:
- Configurable notification levels
- Adaptive heartbeat intervals
- Connection quality monitoring
- First-run setup wizard

## 💬 Support

If you encounter any issues:
1. Check the log file: `client_log.txt`
2. Open an issue on GitHub
3. Provide version (v2.2.4), OS version, and log file

## 🙏 Acknowledgments

Thanks to all users who provided feedback and testing!

---

**Full Changelog**: v2.2.1...v2.2.4
