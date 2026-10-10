#!/usr/bin/env bash
set -euo pipefail
kwriteconfig6 --file kwinrc --group Plugins --key smoothautoscrollEnabled false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect smoothautoscroll
printf '%s\n' 'Smooth Autoscroll disabled.'
