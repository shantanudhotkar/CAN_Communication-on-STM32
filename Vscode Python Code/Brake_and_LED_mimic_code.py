import can
import time
import threading

BITRATE        = 1000000
NODE_ID        = 40
BRAKE_INDEX    = 0x311F
BRAKE_SUBINDEX = 0x00
HEARTBEAT_COB  = 0x700 + NODE_ID  # 0x728
SDO_RESP_COB   = 0x580 + NODE_ID  # 0x5A8

NMT_STATES = {
    0x00: "BOOT-UP",
    0x04: "STOPPED",
    0x05: "OPERATIONAL",
    0x7F: "PRE-OPERATIONAL"
}

# ─── Shared state ────────────────────────────────────────────
node_state      = "UNKNOWN"
last_heartbeat  = None
heartbeat_lock  = threading.Lock()
running         = True

# ─── SDO builder ─────────────────────────────────────────────
def build_sdo_write_u16(node_id, index, subindex, value):
    cob_id = 0x600 + node_id
    data = [
        0x2B,
        index & 0xFF, (index >> 8) & 0xFF,
        subindex & 0xFF,
        value & 0xFF, (value >> 8) & 0xFF,
        0x00, 0x00
    ]
    return can.Message(arbitration_id=cob_id, data=data, is_extended_id=False)

# ─── Brake command ───────────────────────────────────────────
def set_brake(bus, state):
    value = 1 if state else 0
    msg   = build_sdo_write_u16(NODE_ID, BRAKE_INDEX, BRAKE_SUBINDEX, value)
    try:
        bus.send(msg)
        label = "ON  ✓" if value else "OFF ✓"
        print(f"\n  [BRAKE] PB12 → {label}")
    except can.CanError as e:
        print(f"\n  [ERROR] Send failed: {e}")

# ─── Background CAN listener thread ──────────────────────────
def can_listener(bus):
    global node_state, last_heartbeat, running

    while running:
        try:
            msg = bus.recv(timeout=1.0)
            if msg is None:
                continue

            if msg.arbitration_id == HEARTBEAT_COB:
                raw   = msg.data[0]
                state = NMT_STATES.get(raw, f"UNKNOWN(0x{raw:02X})")
                with heartbeat_lock:
                    node_state     = state
                    last_heartbeat = time.time()
                # Print heartbeat on same line without disturbing prompt
                print(f"\r  [HB] Node {NODE_ID} → {state:<20}", end="", flush=True)

            elif msg.arbitration_id == SDO_RESP_COB:
                # SDO response: check for abort (cmd=0x80)
                if msg.data[0] == 0x80:
                    abort_code = int.from_bytes(msg.data[4:8], 'little')
                    print(f"\n  [SDO ABORT] Code: 0x{abort_code:08X}")
                else:
                    print(f"\n  [SDO OK]", end="", flush=True)

        except Exception:
            if running:
                pass  # bus shutting down

# ─── Watchdog: warn if heartbeat lost ────────────────────────
def watchdog():
    global running
    TIMEOUT = 3.0  # seconds — adjust to your OD heartbeat period

    while running:
        time.sleep(1.0)
        with heartbeat_lock:
            if last_heartbeat is not None:
                age = time.time() - last_heartbeat
                if age > TIMEOUT:
                    print(f"\n  [WATCHDOG] ⚠ Heartbeat lost! Last seen {age:.1f}s ago")

# ─── Main ────────────────────────────────────────────────────
def main():
    global running

    print("=" * 50)
    print("  CANopen Brake Controller")
    print(f"  Node {NODE_ID}  |  {BITRATE//1000} kbps")
    print("=" * 50)
    print("Connecting to CANable (gs_usb)...")

    bus = can.Bus(interface="gs_usb", channel=0, bitrate=BITRATE)
    print("Connected!\n")

    # Start background threads
    t_listener = threading.Thread(target=can_listener, args=(bus,), daemon=True)
    t_watchdog  = threading.Thread(target=watchdog, daemon=True)
    t_listener.start()
    t_watchdog.start()

    print("Commands:")
    print("  1 → BRAKE ON  (PB12 HIGH)")
    print("  0 → BRAKE OFF (PB12 LOW)")
    print("  s → Node status")
    print("  q → Quit")
    print("-" * 50 + "\n")

    try:
        while True:
            cmd = input("Command (1/0/s/q): ").strip().lower()

            if cmd == '1':
                set_brake(bus, 1)

            elif cmd == '0':
                set_brake(bus, 0)

            elif cmd == 's':
                with heartbeat_lock:
                    state = node_state
                    age   = f"{time.time() - last_heartbeat:.1f}s ago" if last_heartbeat else "never"
                print(f"\n  [STATUS] Node {NODE_ID} → {state}  (last HB: {age})")

            elif cmd == 'q':
                print("Exiting...")
                break

            else:
                print("  Invalid. Use 1, 0, s, or q")

    except KeyboardInterrupt:
        print("\nStopped by user")

    finally:
        running = False
        bus.shutdown()
        print("Bus disconnected")

if __name__ == "__main__":
    main()