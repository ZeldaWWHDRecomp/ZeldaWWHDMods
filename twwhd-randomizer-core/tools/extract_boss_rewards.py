"""Extract six per-seed boss constants from the player's randomized USA RPX.

This reads data operands only; it never patches or distributes game instructions.
The randomized stage assets provide the matching respawn ACTR item parameters.
"""

# Upstream location_data.yaml Boss paths; first four use cloud r7 high-immediate byte.
BOSS_SITES = (
    ("Gohma", 0x020F5ED4, True),
    ("Kalle Demos", 0x020AF914, True),
    ("Gohdan", 0x020EFE18, False),
    ("Helmaroc King", 0x0206C094, True),
    ("Jalhalla", 0x020DADDC, True),
    ("Molgera", 0x021031E4, False),
)

def extract(read):
    rewards = []
    for name, address, cloud in BOSS_SITES:
        operand = read(address, 4)
        if len(operand) != 4:
            raise ValueError(f"short boss operand for {name}")
        # li r7,(item<<8)|FF for cloud producers; li r4,item for direct producers.
        if operand[:2] != (b"\x38\xE0" if cloud else b"\x38\x80"):
            raise ValueError(f"unsupported boss operand for {name}")
        if cloud and operand[3] != 0xFF or not cloud and operand[2] != 0:
            raise ValueError(f"unsupported boss operand layout for {name}")
        rewards.append(operand[2] if cloud else operand[3])
    return rewards

def main():
    raise SystemExit("Use seed_prepare.py for validated, exclusive seed output; this legacy extractor is a library only.")

if __name__ == "__main__":
    main()
