Screenshots for PixelBit-QuickStart.pdf, taken from the PC emulator (tools/emulator, see tools/README.md).

menu.png, settings-list.png, wifi-page.png, chat-screen.png
    Replay scripts run with: emulator --headless --fast --speed 4 --no-tutorial --replay file
    (the Wi-Fi network name and password on wifi-page.png are random, as on a real console).
lobby-role.png, lobby-find.png
    From images/screens/ (the two-console lobby).

make_quickstart.py turns them into dark-on-white 1-bit pictures to save ink. Run it again after
changing a screenshot or the text: python3 docs/make_quickstart.py (needs LibreOffice).
