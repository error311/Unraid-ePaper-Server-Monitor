#!/bin/bash
# get_status.sh
# This script extracts overall Unraid system metrics,
# Docker stats (and IPs, plus disk size/used/free) for containers 
# ("jellyfin", "jellyseerr", "GluetunVPN", "immich"),
# and VM stats (including IP addresses) for VMs.
# The output is saved as JSON to /mnt/user/appdata/Apache-PHP/www/firmware/status.json

# Define output file path
OUTPUT_FILE="/mnt/user/appdata/Apache-PHP/www/firmware/status.json"

# Get the current date/time formatted as mm-dd-yyyy hh:mm:ss AM/PM (12-hour format)
CURRENT_DATETIME=$(date +"%m-%d-%Y %I:%M:%S %p")

# --------- Unraid System Metrics ---------
LOAD=$(uptime | awk -F'load average: ' '{print $2}' | cut -d',' -f1)
LOAD="${LOAD}%"  # Append "%" sign

if command -v free >/dev/null 2>&1; then
  MEM_TOTAL=$(free -m | awk '/Mem:/{print $2}')
  MEM_USED=$(free -m | awk '/Mem:/{print $3}')
  MEM_FREE=$(free -m | awk '/Mem:/{print $4}')
else
  MEM_TOTAL=$(grep MemTotal /proc/meminfo | awk '{print int($2/1024)}')
  MEM_FREE=$(grep MemFree /proc/meminfo | awk '{print int($2/1024)}')
  MEM_USED=$(( MEM_TOTAL - MEM_FREE ))
fi

DISK_FREE=$(df -h /mnt/user | tail -n1 | awk '{print $4}')
if [ "$DISK_FREE" != "N/A" ]; then
  DISK_FREE="${DISK_FREE}B"
fi

CPU_MODEL=$(grep "model name" /proc/cpuinfo | head -1 | cut -d':' -f2 | xargs)

if [ -f "/mnt/user/system/docker/docker.img" ]; then
  DOCKER_VDISK=$(du -h /mnt/user/system/docker/docker.img | awk '{print $1}')
  DOCKER_VDISK=$(echo "$DOCKER_VDISK" | sed 's/GiB/GB/g')
  if [ "$DOCKER_VDISK" != "N/A" ]; then
    DOCKER_VDISK="${DOCKER_VDISK}B"
  fi
else
  DOCKER_VDISK="N/A"
fi

# --------- Docker Container Stats Function ---------
get_docker_stats() {
  local container="$1"
  if docker ps --format '{{.Names}}' | grep -qi "^${container}$"; then
    local stats
    stats=$(docker stats --no-stream --format '{{.CPUPerc}} {{.MemUsage}}' "${container}")
    local cpu=$(echo "$stats" | awk '{print $1}')
    local mem_usage=$(echo "$stats" | awk '{print $2}')
    local mem_limit=$(echo "$stats" | awk '{print $4}')
    mem_usage=$(echo "$mem_usage" | sed 's/MiB/MB/g; s/GiB/GB/g')
    mem_limit=$(echo "$mem_limit" | sed 's/MiB/MB/g; s/GiB/GB/g')
    echo "$cpu;$mem_usage;$mem_limit"
  else
    echo "N/A;N/A;N/A"
  fi
}

# --------- Function to Get Docker Container IP ---------
get_docker_ip() {
  local container="$1"
  if docker ps --format '{{.Names}}' | grep -qi "^${container}$"; then
    local ip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$container")
    [ -z "$ip" ] && echo "N/A" || echo "$ip"
  else
    echo "N/A"
  fi
}

# --------- Get disk info for a given directory ---------
get_disk_info() {
  # Parameters: directory
  if [ -d "$1" ]; then
    local size=$(df -h "$1" | tail -n1 | awk '{print $2}')
    local used=$(df -h "$1" | tail -n1 | awk '{print $3}')
    local free=$(df -h "$1" | tail -n1 | awk '{print $4}')
    # Append a "B" to each if not "N/A"
    [ "$size" != "N/A" ] && size="${size}B"
    [ "$used" != "N/A" ] && used="${used}B"
    [ "$free" != "N/A" ] && free="${free}B"
    echo "$size;$used;$free"
  else
    echo "N/A;N/A;N/A"
  fi
}

# --------- Get Docker container stats and disk info ---------
# Adjust the directory paths below as needed for each container.
JELLYFIN_STATS=$(get_docker_stats "jellyfin")
JELLYFIN_CPU=$(echo "$JELLYFIN_STATS" | cut -d';' -f1)
JELLYFIN_MEM_USAGE=$(echo "$JELLYFIN_STATS" | cut -d';' -f2)
JELLYFIN_MEM_LIMIT=$(echo "$JELLYFIN_STATS" | cut -d';' -f3)
JELLYFIN_IP=$(get_docker_ip "jellyfin")
JELLYFIN_DISK_INFO=$(get_disk_info "ADDYOURPATH")  # Example directory
JELLYFIN_DISK_SIZE=$(echo "$JELLYFIN_DISK_INFO" | cut -d';' -f1)
JELLYFIN_DISK_USED=$(echo "$JELLYFIN_DISK_INFO" | cut -d';' -f2)
JELLYFIN_DISK_FREE=$(echo "$JELLYFIN_DISK_INFO" | cut -d';' -f3)

JELLYSEERR_STATS=$(get_docker_stats "jellyseerr")
JELLYSEERR_CPU=$(echo "$JELLYSEERR_STATS" | cut -d';' -f1)
JELLYSEERR_MEM_USAGE=$(echo "$JELLYSEERR_STATS" | cut -d';' -f2)
JELLYSEERR_MEM_LIMIT=$(echo "$JELLYSEERR_STATS" | cut -d';' -f3)
JELLYSEERR_IP=$(get_docker_ip "jellyseerr")
JELLYSEERR_DISK_INFO=$(get_disk_info "ADDYOURPATH")  # Example directory
JELLYSEERR_DISK_SIZE=$(echo "$JELLYSEERR_DISK_INFO" | cut -d';' -f1)
JELLYSEERR_DISK_USED=$(echo "$JELLYSEERR_DISK_INFO" | cut -d';' -f2)
JELLYSEERR_DISK_FREE=$(echo "$JELLYSEERR_DISK_INFO" | cut -d';' -f3)

GLUETUN_STATS=$(get_docker_stats "GluetunVPN")
GLUETUN_CPU=$(echo "$GLUETUN_STATS" | cut -d';' -f1)
GLUETUN_MEM_USAGE=$(echo "$GLUETUN_STATS" | cut -d';' -f2)
GLUETUN_MEM_LIMIT=$(echo "$GLUETUN_STATS" | cut -d';' -f3)
GLUETUN_IP=$(get_docker_ip "GluetunVPN")
GLUETUN_DISK_INFO=$(get_disk_info "ADDYOURPATH")  # Example directory
GLUETUN_DISK_SIZE=$(echo "$GLUETUN_DISK_INFO" | cut -d';' -f1)
GLUETUN_DISK_USED=$(echo "$GLUETUN_DISK_INFO" | cut -d';' -f2)
GLUETUN_DISK_FREE=$(echo "$GLUETUN_DISK_INFO" | cut -d';' -f3)

IMMICH_STATS=$(get_docker_stats "immich")
IMMICH_CPU=$(echo "$IMMICH_STATS" | cut -d';' -f1)
IMMICH_MEM_USAGE=$(echo "$IMMICH_STATS" | cut -d';' -f2)
IMMICH_MEM_LIMIT=$(echo "$IMMICH_STATS" | cut -d';' -f3)
IMMICH_IP=$(get_docker_ip "immich")
IMMICH_DISK_INFO=$(get_disk_info "ADDYOURPATH")  # Example directory
IMMICH_DISK_SIZE=$(echo "$IMMICH_DISK_INFO" | cut -d';' -f1)
IMMICH_DISK_USED=$(echo "$IMMICH_DISK_INFO" | cut -d';' -f2)
IMMICH_DISK_FREE=$(echo "$IMMICH_DISK_INFO" | cut -d';' -f3)

# --- Set Online/Offline Status for Each Container ---
if docker ps --format '{{.Names}}' | grep -qi "^jellyfin$"; then
  JELLYFIN_STATUS="online"
else
  JELLYFIN_STATUS="offline"
fi

if docker ps --format '{{.Names}}' | grep -qi "^jellyseerr$"; then
  JELLYSEERR_STATUS="online"
else
  JELLYSEERR_STATUS="offline"
fi

if docker ps --format '{{.Names}}' | grep -qi "^GluetunVPN$"; then
  GLUETUN_STATUS="online"
else
  GLUETUN_STATUS="offline"
fi

if docker ps --format '{{.Names}}' | grep -qi "^immich$"; then
  IMMICH_STATUS="online"
else
  IMMICH_STATUS="offline"
fi

# --------- VM Stats Using virsh ---------
VM_JSON="["
first=1
if command -v virsh >/dev/null 2>&1; then
  while IFS= read -r vm; do
    [ -z "$vm" ] && continue
    VM_INFO=$(virsh dominfo "$vm" 2>/dev/null)
    VM_STATE=$(echo "$VM_INFO" | grep "^State:" | awk '{print $2}')
    VM_CPUS=$(echo "$VM_INFO" | grep "^CPU(s):" | awk '{print $2}')
    VM_MEM_LIMIT_KB=$(echo "$VM_INFO" | grep "^Max memory:" | awk '{print $3}')
    if [ -n "$VM_MEM_LIMIT_KB" ]; then
      VM_MEM_LIMIT_MB=$(( VM_MEM_LIMIT_KB / 1024 ))
    else
      VM_MEM_LIMIT_MB="N/A"
    fi
    VM_AUTOSTART=$(echo "$VM_INFO" | grep "^Autostart:" | awk '{print $2}')
    VM_IP=$(virsh domifaddr "$vm" --source agent 2>/dev/null | grep -m 1 ipv4 | awk '{print $4}' | cut -d'/' -f1)
    if [ -z "$VM_IP" ]; then
      VM_IP="N/A"
    fi
    if [ $first -eq 1 ]; then
      first=0
    else
      VM_JSON+=","
    fi
    VM_JSON+="{\"name\":\"$vm\",\"state\":\"$VM_STATE\",\"cpus\":\"$VM_CPUS\",\"max_memory_mb\":\"$VM_MEM_LIMIT_MB\",\"autostart\":\"$VM_AUTOSTART\",\"ip\":\"$VM_IP\"}"
  done < <(virsh list --all --name)
  VM_JSON+="]"
else
  VM_JSON="[]"
fi

# --------- Output as JSON ---------
cat <<EOF > "$OUTPUT_FILE"
{
  "timestamp": "$CURRENT_DATETIME",
  "server": {
    "cpu_load": "$LOAD",
    "cpu_model": "$CPU_MODEL",
    "memory": {
      "total_mb": "$MEM_TOTAL",
      "used_mb": "$MEM_USED",
      "free_mb": "$MEM_FREE"
    },
    "disk_free": "$DISK_FREE",
    "docker_vdisk_size": "$DOCKER_VDISK"
  },
  "jellyfin": {
    "status": "$JELLYFIN_STATUS",
    "cpu": "$JELLYFIN_CPU",
    "memory": {
      "usage": "$JELLYFIN_MEM_USAGE",
      "limit": "$JELLYFIN_MEM_LIMIT"
    },
    "disk_size": "$JELLYFIN_DISK_SIZE",
    "disk_used": "$JELLYFIN_DISK_USED",
    "disk_free": "$JELLYFIN_DISK_FREE",
    "ip": "$JELLYFIN_IP"
  },
  "jellyseerr": {
    "status": "$JELLYSEERR_STATUS",
    "cpu": "$JELLYSEERR_CPU",
    "memory": {
      "usage": "$JELLYSEERR_MEM_USAGE",
      "limit": "$JELLYSEERR_MEM_LIMIT"
    },
    "disk_size": "$JELLYSEERR_DISK_SIZE",
    "disk_used": "$JELLYSEERR_DISK_USED",
    "disk_free": "$JELLYSEERR_DISK_FREE",
    "ip": "$JELLYSEERR_IP"
  },
  "gluetunvpn": {
    "status": "$GLUETUN_STATUS",
    "cpu": "$GLUETUN_CPU",
    "memory": {
      "usage": "$GLUETUN_MEM_USAGE",
      "limit": "$GLUETUN_MEM_LIMIT"
    },
    "disk_size": "$GLUETUN_DISK_SIZE",
    "disk_used": "$GLUETUN_DISK_USED",
    "disk_free": "$GLUETUN_DISK_FREE",
    "ip": "$GLUETUN_IP"
  },
  "immich": {
    "status": "$IMMICH_STATUS",
    "cpu": "$IMMICH_CPU",
    "memory": {
      "usage": "$IMMICH_MEM_USAGE",
      "limit": "$IMMICH_MEM_LIMIT"
    },
    "disk_size": "$IMMICH_DISK_SIZE",
    "disk_used": "$IMMICH_DISK_USED",
    "disk_free": "$IMMICH_DISK_FREE",
    "ip": "$IMMICH_IP"
  },
  "vms": $VM_JSON
}
EOF

echo "Status JSON saved to $OUTPUT_FILE"
