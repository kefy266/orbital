#!/bin/bash
# Orbital script tool: arguments arrive as JSON on stdin, stdout is the result.
cat > /dev/null
echo "OS: $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-$(uname -s)}")"
echo "Kernel: $(uname -r)"
echo "CPU: $(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs) ($(nproc) çekirdek)"
echo
free -h 2>/dev/null
echo
df -h --output=target,size,used,avail,pcent / "$HOME" 2>/dev/null | uniq
