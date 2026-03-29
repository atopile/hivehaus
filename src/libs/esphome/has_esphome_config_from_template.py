from pathlib import Path
import re
from typing import Any

from faebryk.core.graph import GraphFunctions
from faebryk.core.node import once
from faebryk.core.parameter import Parameter
import yaml

import faebryk.library._F as F
from faebryk.libs.util import dict_value_visitor
from yaml.constructor import ConstructorError

from .has_esphome_config import has_esphome_config

SUBST_RE = re.compile(r"^\{\{(.*)\}\}$")


class has_esphome_config_from_template(has_esphome_config.impl()):
    def __init__(self, path: str):
        super().__init__()
        self._path = path

    lazy: F.is_lazy

    @property
    @once
    def path(self) -> Path:
        from atopile.front_end import from_dsl

        if (from_dsl_ := self.try_get_trait(from_dsl)) is None:
            raise ValueError(
                "No source context found for module with has_esphome_config_from_template trait"
            )

        if from_dsl_.src_file is None:
            raise ValueError(
                "No source file found for module with has_esphome_config_from_template trait"
            )

        return from_dsl_.src_file.parent / self._path

    def on_obj_set(self):
        super().on_obj_set()

        if not self.path.exists():
            raise FileNotFoundError(f"Template file not found: {self.path}")

        if not self.path.is_file():
            raise FileNotFoundError(f"Template file is not a file: {self.path}")

        if not self.path.suffixes == [".yaml", ".tmpl"]:
            raise ValueError(f"File is not a YAML template file: {self.path}")

    def get_config(self) -> dict:
        params = {
            p.get_full_name(): p
            for p in GraphFunctions(self.get_graph()).nodes_of_type(Parameter)
        }

        try:
            config = yaml.safe_load(self.path.read_text()) or {}
        except (yaml.YAMLError, ConstructorError) as e:
            raise ValueError(f"Error parsing YAML file: {e}") from e

        def sub_param(v: Any) -> Any:
            # TODO: this is fragile
            if isinstance(v, str) and (match := SUBST_RE.match(v)):
                return params[match.group(1).strip()]

            return v

        dict_value_visitor(config, lambda _, v: sub_param(v))
        return config
