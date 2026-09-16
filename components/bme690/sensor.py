import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DURATION,
    CONF_GAS_RESISTANCE,
    CONF_HEATER,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_GAS_CYLINDER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_OHM,
    UNIT_PERCENT,
)

CODEOWNERS = ["@SolderedElectronics"]
DEPENDENCIES = ["i2c"]

CONF_MULTIPLIER = "multiplier"
CONF_ODR = "odr"
CONF_OPERATION_MODE = "operation_mode"
CONF_PROFILE = "profile"
CONF_SHARED_DURATION = "shared_duration"

MODE_FORCED = "forced"
MODE_PARALLEL = "parallel"
MODE_SEQUENTIAL = "sequential"

# The sensor holds at most ten heater profile steps.
MAX_PROFILE_LEN = 10

bme690_ns = cg.esphome_ns.namespace("bme690")
BME690Component = bme690_ns.class_(
    "BME690Component", cg.PollingComponent, i2c.I2CDevice
)

BME690Oversampling = bme690_ns.enum("BME690Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": BME690Oversampling.BME690_OVERSAMPLING_NONE,
    "1X": BME690Oversampling.BME690_OVERSAMPLING_1X,
    "2X": BME690Oversampling.BME690_OVERSAMPLING_2X,
    "4X": BME690Oversampling.BME690_OVERSAMPLING_4X,
    "8X": BME690Oversampling.BME690_OVERSAMPLING_8X,
    "16X": BME690Oversampling.BME690_OVERSAMPLING_16X,
}

BME690IIRFilter = bme690_ns.enum("BME690IIRFilter")
IIR_FILTER_OPTIONS = {
    "OFF": BME690IIRFilter.BME690_IIR_FILTER_OFF,
    "1X": BME690IIRFilter.BME690_IIR_FILTER_1X,
    "3X": BME690IIRFilter.BME690_IIR_FILTER_3X,
    "7X": BME690IIRFilter.BME690_IIR_FILTER_7X,
    "15X": BME690IIRFilter.BME690_IIR_FILTER_15X,
    "31X": BME690IIRFilter.BME690_IIR_FILTER_31X,
    "63X": BME690IIRFilter.BME690_IIR_FILTER_63X,
    "127X": BME690IIRFilter.BME690_IIR_FILTER_127X,
}

BME690ODR = bme690_ns.enum("BME690ODR")
ODR_OPTIONS = {
    "0.59ms": BME690ODR.BME690_ODR_0_59_MS,
    "10ms": BME690ODR.BME690_ODR_10_MS,
    "20ms": BME690ODR.BME690_ODR_20_MS,
    "62.5ms": BME690ODR.BME690_ODR_62_5_MS,
    "125ms": BME690ODR.BME690_ODR_125_MS,
    "250ms": BME690ODR.BME690_ODR_250_MS,
    "500ms": BME690ODR.BME690_ODR_500_MS,
    "1000ms": BME690ODR.BME690_ODR_1000_MS,
    "none": BME690ODR.BME690_ODR_NONE,
}

BME690OperationMode = bme690_ns.enum("BME690OperationMode")
OPERATION_MODE_OPTIONS = {
    MODE_FORCED: BME690OperationMode.BME690_OPERATION_MODE_FORCED,
    MODE_PARALLEL: BME690OperationMode.BME690_OPERATION_MODE_PARALLEL,
    MODE_SEQUENTIAL: BME690OperationMode.BME690_OPERATION_MODE_SEQUENTIAL,
}

GAS_RESISTANCE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_OHM,
    icon=ICON_GAS_CYLINDER,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

HEATER_PROFILE_STEP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.int_range(min=0, max=400),
        cv.Optional(CONF_DURATION): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=4032)),
        ),
        cv.Optional(CONF_MULTIPLIER): cv.int_range(min=1, max=255),
        cv.Optional(CONF_GAS_RESISTANCE): GAS_RESISTANCE_SCHEMA,
    }
)

HEATER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEMPERATURE, default=320): cv.int_range(min=0, max=400),
        cv.Optional(CONF_DURATION, default="150ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=4032)),
        ),
        cv.Optional(CONF_SHARED_DURATION, default="140ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=1923)),
        ),
        cv.Optional(CONF_PROFILE): cv.All(
            cv.ensure_list(HEATER_PROFILE_STEP_SCHEMA),
            cv.Length(min=1, max=MAX_PROFILE_LEN),
        ),
    }
)


def _validate_operation_mode(config):
    """Makes sure the heater configuration matches the selected operation mode."""
    mode = config[CONF_OPERATION_MODE]
    heater = config[CONF_HEATER]
    profile = heater.get(CONF_PROFILE)

    if mode == MODE_FORCED:
        if profile is not None:
            raise cv.Invalid(
                f"A heater '{CONF_PROFILE}' is only used in the '{MODE_PARALLEL}' and "
                f"'{MODE_SEQUENTIAL}' operation mode, use '{CONF_TEMPERATURE}' and "
                f"'{CONF_DURATION}' in the '{MODE_FORCED}' one",
                path=[CONF_HEATER, CONF_PROFILE],
            )
        return config

    if profile is None:
        raise cv.Invalid(
            f"The '{mode}' operation mode requires a heater '{CONF_PROFILE}'",
            path=[CONF_HEATER],
        )

    for index, step in enumerate(profile):
        if mode == MODE_SEQUENTIAL:
            missing, unexpected = CONF_DURATION, CONF_MULTIPLIER
        else:
            missing, unexpected = CONF_MULTIPLIER, CONF_DURATION
        if missing not in step:
            raise cv.Invalid(
                f"Every heater profile step needs a '{missing}' in the '{mode}' operation mode",
                path=[CONF_HEATER, CONF_PROFILE, index],
            )
        if unexpected in step:
            raise cv.Invalid(
                f"A heater profile step has no '{unexpected}' in the '{mode}' operation mode",
                path=[CONF_HEATER, CONF_PROFILE, index, unexpected],
            )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME690Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="2X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="1X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_GAS_RESISTANCE): GAS_RESISTANCE_SCHEMA,
            cv.Optional(CONF_OPERATION_MODE, default=MODE_FORCED): cv.one_of(
                *OPERATION_MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
                IIR_FILTER_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ODR, default="0.59ms"): cv.enum(ODR_OPTIONS, lower=True),
            cv.Optional(CONF_HEATER, default={}): HEATER_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x76)),
    _validate_operation_mode,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_operation_mode(OPERATION_MODE_OPTIONS[config[CONF_OPERATION_MODE]]))
    cg.add(var.set_iir_filter(config[CONF_IIR_FILTER]))
    cg.add(var.set_odr(config[CONF_ODR]))

    if conf := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling(conf[CONF_OVERSAMPLING]))

    if conf := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling(conf[CONF_OVERSAMPLING]))

    if conf := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_humidity_sensor(sens))
        cg.add(var.set_humidity_oversampling(conf[CONF_OVERSAMPLING]))

    if conf := config.get(CONF_GAS_RESISTANCE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_gas_resistance_sensor(sens))

    heater = config[CONF_HEATER]
    cg.add(var.set_heater_temperature(heater[CONF_TEMPERATURE]))
    cg.add(var.set_heater_duration(heater[CONF_DURATION].total_milliseconds))
    cg.add(var.set_shared_heater_duration(heater[CONF_SHARED_DURATION].total_milliseconds))

    for index, step in enumerate(heater.get(CONF_PROFILE, [])):
        if CONF_DURATION in step:
            duration = step[CONF_DURATION].total_milliseconds
        else:
            duration = step[CONF_MULTIPLIER]
        cg.add(var.add_heater_profile_step(step[CONF_TEMPERATURE], duration))

        if conf := step.get(CONF_GAS_RESISTANCE):
            sens = await sensor.new_sensor(conf)
            cg.add(var.set_profile_gas_resistance_sensor(index, sens))
