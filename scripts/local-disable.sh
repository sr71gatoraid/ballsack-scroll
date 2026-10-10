#!/usr/bin/env bash
set -euo pipefail
kwriteconfig6 --file kwinrc --group Plugins --key ballsackscrollEnabled false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect ballsackscroll
printf '%s\n' 'Ballsack Scroll disabled.'
