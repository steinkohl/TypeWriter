#!/bin/sh
# Install typewriterd on a Raspberry Pi (Raspberry Pi OS Bookworm or newer).
# Run as root from this directory:  sudo ./install.sh [--cups]
set -eu

cd "$(dirname "$0")"

id typewriter >/dev/null 2>&1 || useradd --system --groups dialout --no-create-home typewriter

apt-get install -y python3-venv
python3 -m venv /opt/typewriter/venv
/opt/typewriter/venv/bin/pip install --upgrade .
ln -sf /opt/typewriter/venv/bin/typewriter-print /usr/local/bin/typewriter-print

install -m 644 systemd/typewriterd.service /etc/systemd/system/typewriterd.service
systemctl daemon-reload
systemctl enable --now typewriterd

if [ "${1:-}" = "--cups" ]; then
	# CUPS queue "Typewriter": converts documents to plain text and
	# hands them to typewriterd.
	apt-get install -y cups cups-filters
	lpadmin -p Typewriter -E -v socket://localhost:9100 \
		-m lsb/usr/cupsfilters/textonly.ppd \
		-D "Panasonic KX-R191" -o printer-is-shared=true
	echo "CUPS queue 'Typewriter' created: lp -d Typewriter file.txt"
fi

echo "Done. Test with: echo 'Hello world' | typewriter-print"
