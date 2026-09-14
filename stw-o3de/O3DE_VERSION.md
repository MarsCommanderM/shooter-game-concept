# Pinned O3DE revision

| Field | Value |
|---|---|
| O3DE release | 26.05.0 |
| Git tag | `2605.0` |
| Git commit | `3db6943249d8bd7960b9ed7e9aee310b7668586e` |
| Upstream | `https://github.com/o3de/o3de.git` |
| Upstream commit date | 2026-05-27 |
| Selected for STW gate | 2026-08-22 |
| Source licenses | Apache-2.0 OR MIT (upstream) |

Verify the pin without trusting a moving branch:

```bash
git ls-remote --tags https://github.com/o3de/o3de.git refs/tags/2605.0
```

The result must be exactly:

```text
3db6943249d8bd7960b9ed7e9aee310b7668586e refs/tags/2605.0
```

## Upstream requirements for this pin

The following values come from the O3DE 26.05 documentation and release
notes. They are requirements for the gate host, not dependencies added to the
legacy STW executable.

| Requirement | Pinned gate value |
|---|---|
| Host | x86-64 Linux; Ubuntu 22.04 LTS is the primary documented distribution, with Ubuntu 24.04 package guidance also provided |
| Compiler | Clang 14 or later; use Clang 18 on Ubuntu 24.04 |
| CMake | 3.30.0 or later |
| Generator | Ninja Multi-Config recommended |
| RAM | 16 GB minimum, 32 GB recommended; source builds need additional memory per parallel compile thread |
| Free disk | 100+ GB minimum for a source build; more may be required by project configuration |
| GPU | Real DirectX 12- or Vulkan-compatible GPU, minimum 2 GB VRAM |
| Shader capability | Shader Model 6.2 baseline; Shader Model 6.3 only for optional ray tracing |
| Linux graphics | Current vendor Vulkan driver and a working Vulkan ICD |

Hardware ray tracing is optional and is not part of the STW gate. DLSS is not
assumed or claimed.

## Primary sources

- [O3DE 26.05.0 release notes](https://www.docs.o3de.org/docs/release-notes/2605-0-release-notes/)
- [O3DE system requirements](https://www.docs.o3de.org/docs/welcome-guide/requirements/)
- [Building O3DE on Linux](https://docs.o3de.org/docs/welcome-guide/setup/setup-from-github/building-linux/)
- [Creating an O3DE project on Linux](https://docs.o3de.org/docs/welcome-guide/create/creating-projects-using-cli/creating-linux/)

Do not update this pin by editing only the version label. A future upgrade must
record and verify a new exact upstream commit and repeat the complete gate.
