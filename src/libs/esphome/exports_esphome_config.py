import faebryk.library._F as F
from faebryk.core.module import Module
from faebryk.core.solver.solver import Solver
from atopile.config import config


class exports_esphome_config(F.provides_build_target.impl()):
    name = "esphome-config"

    def run(self, app: Module, solver: Solver) -> None:
        """Generate an ESPHome configuration file."""
        from faebryk.exporters.esphome import esphome

        esphome_config = esphome.make_esphome_config(app.get_graph(), solver)
        config_path = config.build.paths.output_base.with_suffix(".esphome.yaml")

        with config_path.open("w", encoding="utf-8") as f:
            f.write(esphome.dump_esphome_config(esphome_config))
