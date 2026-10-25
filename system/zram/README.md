# zram: compressed swap in memory

Files the image build (`.github/workflows/main.yml`, step "zram compressed
swap") adds to the system from AetherX6100Buildroot:

- `linux.config`: kernel options, appended to the board's kernel config.
- `S05zram`: start-up script, installed as `/etc/init.d/S05zram`. It makes
  a zram swap of half the RAM at boot.

They belong in AetherX6100Buildroot itself
(`br2_external/board/X6100/linux/`); they're here so a test build can carry
them. "Speak memory" (voice key + U) says how much is in use.
