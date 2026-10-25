ATLAS
=====

PLAYING
  Double-click Atlas.exe. The main menu has Skirmish, Host, Join,
  Settings and Controls.

  If Windows says "Windows protected your PC", click "More info" and
  then "Run anyway". The game isn't code-signed, so Windows warns about
  every new build.

  Keep the Assets folder and SDL3.dll next to Atlas.exe.

CO-OP (up to 4 players)
  Host:  double-click "Host co-op.bat" (or Host on the main menu).
         Allow Atlas through the Windows firewall when it asks.
  Join:  double-click "Join co-op.bat" and type the host's address
         (or Join on the main menu).

  Same house / LAN: use the host PC's local IP (run "ipconfig" on the
  host and look for IPv4 Address, e.g. 192.168.1.20).

  Over the internet: the host forwards UDP port 27960 on their router
  to their PC and shares their public IP, or both players join the
  same virtual LAN (Tailscale, ZeroTier, Radmin VPN) and use that IP.

  Everyone needs the same build. Players can join a match already in
  progress.

CONTROLS
  A / D            move
  Space            jump
  S                crouch
  W / Left Shift   jetpack
  Mouse            aim
  Left mouse       fire
  Right mouse      throw grenade
  1-6 / wheel      switch item
  R                reload
  Tab (hold)       buy menu
  Q                switch to next friendly body
  Z / X / C        squad order: defend / follow / attack
  M                minimap
  Esc              pause menu

Settings are saved in %APPDATA%\Atlas\Atlas\settings.ini.
