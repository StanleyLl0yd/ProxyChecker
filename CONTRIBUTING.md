# Development workflow

1. Create a feature branch from `main`.
2. Implement one coherent milestone or issue. Keep native Windows dependencies minimal.
3. Add relevant tests and diagnostic evidence (never credentials).
4. Open a pull request; let `build-windows` pass.
5. Squash-merge into protected `main`. No direct pushes, force pushes or branch deletion.

Follow the architecture and security requirements in `docs/ARCHITECTURE.md` and `SECURITY.md`.
