from faebryk.core.cpp import Graph
from faebryk.core.graph import GraphFunctions
from faebryk.core.parameter import Parameter
import faebryk.library._F as F
from faebryk.core.module import Module
from faebryk.core.solver.solver import Solver
from atopile.config import config
from faebryk.libs.units import Quantity
from faebryk.libs.util import cast_assert, dict_value_visitor, merge_dicts
import yaml


class exports_esphome_config(F.provides_build_target.impl()):
    name = "esphome-config"

    @staticmethod
    def _make_esphome_config(G: Graph, solver: Solver) -> dict:
        esphome_components = GraphFunctions(G).nodes_with_trait(F.has_esphome_config)

        esphome_config = merge_dicts(*[t.get_config() for _, t in esphome_components])

        # deep find parameters in dict and solve
        def solve_parameter(v):
            if not isinstance(v, Parameter):
                return v

            return str(
                cast_assert(Quantity, solver.get_any_single(v, lock=True)).magnitude
            )

        dict_value_visitor(esphome_config, lambda _, v: solve_parameter(v))

        return esphome_config

    def run(self, app: Module, solver: Solver) -> None:
        """Generate an ESPHome configuration file."""
        esphome_config = self._make_esphome_config(app.get_graph(), solver)
        config_path = config.build.paths.output_base.with_suffix(".esphome.yaml")

        with config_path.open("w", encoding="utf-8") as f:
            f.write(yaml.dump(esphome_config))
