#!/bin/bash
IFACE=${1:-eth0}
DOWN=$2
UP=$3
CMD=$4

if [[ "$CMD" == "stop" ]]; then
    echo "[INFO] 清除限速..."
    tc qdisc del dev $IFACE root 2>/dev/null
    tc qdisc del dev $IFACE ingress 2>/dev/null
    tc qdisc del dev ifb0 root 2>/dev/null
    ip link set dev ifb0 down 2>/dev/null
    echo "[INFO] 限速已清除。"
    exit 0
fi

modprobe ifb 2>/dev/null
ip link add ifb0 type ifb 2>/dev/null || true
ip link set dev ifb0 up 2>/dev/null

tc qdisc del dev $IFACE root 2>/dev/null
tc qdisc del dev $IFACE ingress 2>/dev/null
tc qdisc del dev ifb0 root 2>/dev/null

echo "[INFO] 设置上传限速 ${UP} Mbps"
tc qdisc add dev $IFACE root handle 1: htb default 10 r2q 4
tc class add dev $IFACE parent 1: classid 1:10 htb rate ${UP}mbit ceil ${UP}mbit quantum 1500

echo "[INFO] 设置下载限速 ${DOWN} Mbps（通过 ifb0 实现）"
tc qdisc add dev $IFACE handle ffff: ingress
tc filter add dev $IFACE parent ffff: protocol ip u32 match u32 0 0 action mirred egress redirect dev ifb0
tc qdisc add dev ifb0 root handle 1: htb default 10 r2q 4
tc class add dev ifb0 parent 1: classid 1:10 htb rate ${DOWN}mbit ceil ${DOWN}mbit quantum 1500

echo "[OK] 已完成限速：$IFACE ⇄ DOWN=$DOWN Mbps / UP=$UP Mbps"
