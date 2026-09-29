import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = []

ppp_modem_ns = cg.esphome_ns.namespace("ppp_modem")
PppModemComponent = ppp_modem_ns.class_("PppModemComponent", cg.Component)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PppModemComponent),
        cv.Optional("apn", default="internet"): cv.string,
    }
)

def to_code(config):
    var = cg.new_variable(config[CONF_ID], PppModemComponent())
    yield cg.register_component(cg.RawExpression(f"&{config[CONF_ID]}"), config)
    
    if "apn" in config:
        cg.add(var.set_apn(config["apn"]))
