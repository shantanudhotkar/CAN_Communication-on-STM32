import can
import time

BITRATE  = 1000000
NODE_ID  = 40

# OD index is the same 0x311F — same object dictionary
BRAKE_INDEX    = 0x311F
BRAKE_SUBINDEX = 0x00

def build_sdo_write_u16(node_id, index, subindex, value):
    cob_id = 0x600 + node_id
    cmd    = 0x2B
    idx_lo = index & 0xFF
    idx_hi = (index >> 8) & 0xFF
    sub    = subindex & 0xFF
    val_lo = value & 0xFF
    val_hi = (value >> 8) & 0xFF
    data   = [cmd, idx_lo, idx_hi, sub, val_lo, val_hi, 0x00, 0x00]
    return can.Message(
        arbitration_id = cob_id,
        data           = data,
        is_extended_id = False
    )

def set_brake(bus, node_id, state):
    value = 1 if state else 0
    msg   = build_sdo_write_u16(node_id, BRAKE_INDEX, BRAKE_SUBINDEX, value)
    bus.send(msg)
    print(f"  Sent BRAKE_IN (PB12) = {value} ({'ON' if value else 'OFF'})")
    time.sleep(0.1)

def main():
    print("Connecting to CANable (gs_usb)...")
    bus = can.Bus(
        interface = "gs_usb",
        channel   = 0,
        bitrate   = BITRATE
    )
    print("Connected!\n")
    print("Commands:")
    print("  1 → BRAKE ON  (PB12 HIGH)")
    print("  0 → BRAKE OFF (PB12 LOW)")
    print("  q → Quit\n")

    try:
        while True:
            cmd = input("Enter command (1/0/q): ").strip()
            if cmd == '1':
                set_brake(bus, NODE_ID, 1)
            elif cmd == '0':
                set_brake(bus, NODE_ID, 0)
            elif cmd == 'q':
                print("Exiting...")
                break
            else:
                print("  Invalid option. Use 1, 0, or q")

    except KeyboardInterrupt:
        print("\nStopped")
    finally:
        bus.shutdown()
        print("Bus disconnected")

if __name__ == "__main__":
    main()