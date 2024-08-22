# Pistachio Engine

A Vulkan/D3D12(currently broken) game engine written in c++ 20, its very much incomplete

## Usage

It is intended as a meson subproject so within your subprojects directory you could create a wrap-git, or add a submodule like so:

```bash
git submodule add https://github.com/Git-i/Pistachio-Engine.git
```

Then in your top-level `meson.build` add it as a subproject and retrieve the dependency object(`pistachio_dep`).

```meson
pistachio_dep = pistachio_engine.get_variable('pistachio_dep')
```



this can also be done via meson wrap, using provide `dependency_name = pistachio_dep` and then the `dependency('dependency_name')`
