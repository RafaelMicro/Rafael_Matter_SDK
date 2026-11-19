# Rafael Submodules

This project uses one Git submodule from Rafael, the RT58x SDK components. The
OpenThread protocol stack is a nested submodule inside it.

## 1. Rafael SDK Components (`third_party/rafael/repo/components`)

- **Remote**: `https://github.com/RafaelMicro/components.git`
- **Path**: `third_party/rafael/repo/components/`
- **Description**: Rafael peripheral component drivers and platform support
  libraries
- **Nested submodule**: `network/thread` (see below), so this submodule must be
  checked out recursively. It is marked `recursive = true` in `.gitmodules`, so
  `scripts/checkout_submodules.py` handles it. When updating by hand, run:

  ```bash
  git submodule update --init --recursive third_party/rafael/repo/components
  ```

## 2. OpenThread (`third_party/rafael/repo/components/network/thread`)

- **Remote**: `git@github.com:RafaelMicro/openthread.git` (registered in the
  components repository's `.gitmodules`)
- **Path**: `third_party/rafael/repo/components/network/thread/`
- **Access path**: `third_party/openthread/components-thread/`, a symbolic link
  to the path above. The GN build files (`src/platform/RT58x/args.gni`,
  `third_party/openthread/platforms/rt58x/BUILD.gn`,
  `third_party/rafael/BUILD.gn`) reference OpenThread through this link.
- **Description**: Rafael's customized OpenThread implementation, containing:
  - `openthread/` — OpenThread core source code
  - `openthread_port/` — Rafael platform port layer
- **Version**: The OpenThread commit is pinned by the components repository.
  To change it, update the submodule inside `components`, commit there, then
  update the `components` pointer in this repository.
