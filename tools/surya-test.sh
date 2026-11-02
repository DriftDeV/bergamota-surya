#!/bin/sh
# One RAM-boot test cycle on the Poco X3 NFC (surya), no buttons needed when Enxerto is in `boot`.
#   tools/surya-test.sh IMAGE LOGNAME [SECONDS]
# Waits for a fastboot device (Bergamota `bergamota0001`, or the Xiaomi ABL one), starts the UDP log
# capture (port 5140), `fastboot boot IMAGE`, then waits for the USB ethernet (en6), the ping and the
# end of the capture. Prints the SWEET GPU lines at the end. Writes logs/LOGNAME.txt (git-ignored).
# Never uses `getvar` against the Xiaomi ABL.
set -u
IMG=${1:?image}; NAME=${2:?log name}; SECS=${3:-300}
cd "$(dirname "$0")/.." || exit 1
LOG=logs/$NAME.txt
mkdir -p logs
# The phone may still be running iOS from the previous test: reboot it over SSH so it comes back in Bergamota fastboot.
if [ -z "$(fastboot devices)" ] && ping -c1 -t2 192.168.7.2 >/dev/null 2>&1; then
	echo "iOS answers on 192.168.7.2 and no fastboot device: rebooting it over SSH"
	ssh -o BatchMode=yes -o ConnectTimeout=6 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
		root@192.168.7.2 '/sbin/reboot || /usr/sbin/reboot || reboot' >/dev/null 2>&1 || true
	sleep 8
fi
echo "waiting for a fastboot device..."
for i in $(seq 1 120); do
	DEV=$(fastboot devices | awk 'NR==1{print $1}')
	[ -n "$DEV" ] && break
	sleep 2
done
[ -n "${DEV:-}" ] || { echo "no fastboot device after 240 s: hold Power+VolDown"; exit 2; }
echo "device $DEV"
pkill -f surya-cap.py 2>/dev/null
python3 -I tools/surya-cap.py "$LOG" "$SECS" >/dev/null 2>&1 &
CAP=$!
sleep 1
fastboot -s "$DEV" boot "$IMG" 2>&1 | tail -3
for i in $(seq 1 40); do ifconfig en6 >/dev/null 2>&1 && { echo "en6 up after ~$((i*3)) s"; break; }; sleep 3; done
wait $CAP
echo "log: $LOG ($(wc -c < "$LOG") bytes)"
grep -a "SWEET GPU" "$LOG" | cut -c1-230 | tail -${GPU_LINES:-80}
echo "--- phone after the run:"
fastboot devices
