#!/bin/sh
#
# Mute board audio output early during boot to avoid startup sound.
#

mute_outputs() {
    tinymix set "SPK Switch" 0 >/dev/null 2>&1
    tinymix set "LINEOUT Switch" 0 >/dev/null 2>&1
    tinymix set "DAC volume" 0 >/dev/null 2>&1
    tinymix set "LINEOUT volume" 0 >/dev/null 2>&1
}

case "$1" in
    start|boot)
        mute_outputs
        ;;
    stop)
        ;;
    restart|reload)
        mute_outputs
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
