/****************************************************************************
 *
 * (c) 2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "HealthAndArmingChecks.h"

#include <QGCLoggingCategory.h>

QGC_LOGGING_CATEGORY(HealthAndArmingChecks, "HealthAndArmingChecks");

using health_component_t = events::common::enums::health_component_t;
using navigation_mode_category_t = events::common::enums::navigation_mode_category_t;

void HealthAndArmingCheckHandler::handleEvent(const events::parser::ParsedEvent& event)
{
    Type type;
    if (event.eventNamespace() == "common" && event.name() == "arming_check_summary") {
        type = Type::ArmingCheckSummary;
    } else if (event.eventNamespace() == "common" && event.name() == "health_summary") {
        type = Type::HealthSummary;
    } else {
        type = Type::Other;
    }

    // the expected order of receiving is:
    // - ArmingCheckSummary
    // - N Other
    // - HealthSummary

    if (type != _expected_event) {
        if (_expected_event == Type::Other && type == Type::HealthSummary) {
            // all good
        } else if (type == Type::ArmingCheckSummary) {
            qCDebug(HealthAndArmingChecks) << "Unexpected ArmingCheckSummary event, resetting. Expected:" << (int)_expected_event;
            // accept & reset
        } else {
            qCDebug(HealthAndArmingChecks) << "Unexpected event, resetting. Expected:" << (int)_expected_event
                    << "Got:" << (int)type;
            _expected_event = Type::ArmingCheckSummary;
            return;
        }
    }

    switch (type) {
        case Type::ArmingCheckSummary:
            reset();
            if (event.id() == (uint32_t)events::common::event_id_t::arming_check_summary) {
                ArmingCheckSummary &arming = _results[_current_result].arming;
                events::common::decode_arming_check_summary(event.eventData(), arming.error, arming.warning, arming.can_arm);
                _expected_event = Type::Other;
            }
            break;
        case Type::Other: {
            Check check;
            check.type = event.group() == "health" ? CheckType::Health : CheckType::ArmingCheck;
            check.message = QString::fromStdString(event.message());
            check.description = QString::fromStdString(event.description());
            check.affected_modes = (events::common::enums::navigation_mode_category_t)event.argumentValue(0).value.val_uint8_t;
            check.affected_health_component_index = event.argumentValue(1).value.val_uint8_t;
            check.log_level = events::externalLogLevel(event.eventData().log_levels);
            _results[_current_result].checks.append(check);
        }
            break;
        case Type::HealthSummary:
            if (event.id() == (uint32_t)events::common::event_id_t::health_summary) {
                HealthSummary &health = _results[_current_result].health;
                events::common::decode_health_summary(event.eventData(), health.is_present, health.error, health.warning);
                _current_result = (_current_result + 1) % 2;
                emit update();
                testReport();
            }
            reset();
            break;
    }
}

void HealthAndArmingCheckHandler::reset()
{
	_results[_current_result].reset();
	_expected_event = Type::ArmingCheckSummary;
}

void HealthAndArmingCheckHandler::testReport()
{
    // just for testing...
    qWarning() << "Got Health/Arming checks update";
    qWarning() << "Arming possible in current mode: " << (results().arming.can_arm & navigation_mode_category_t::current);
    qWarning() << "Can a mission be flown: " << (results().arming.can_arm & navigation_mode_category_t::mission);
    qWarning() << "Autonomous (e.g. Takeoff) flight possible: " << (results().arming.can_arm & navigation_mode_category_t::autonomous);

    QString gps_icon_color;
    if ((results().health.error & health_component_t::sensor_gps) ||
            (results().health.error & health_component_t::global_position_estimate) ||
            (results().arming.error & health_component_t::sensor_gps) ||
            (results().arming.error & health_component_t::global_position_estimate)) {
        gps_icon_color = "red";
    } else if((results().health.warning & health_component_t::sensor_gps) ||
            (results().health.warning & health_component_t::global_position_estimate) ||
            (results().arming.warning & health_component_t::sensor_gps) ||
            (results().arming.warning & health_component_t::global_position_estimate)) {
        gps_icon_color = "yellow";
    } else {
        gps_icon_color = "green";
    }
    qWarning() << "GPS/Position icon color: " << gps_icon_color;

    // display events that are relevant for current mode:
    qWarning() << "Current flight mode:";
    for (const auto& check : results().checks) {
        if (check.affected_modes & navigation_mode_category_t::current) {
            qWarning() << "  " << (int)check.log_level << check.message;
        }
    }
    qWarning() << "Other flight modes:";
    for (const auto& check : results().checks) {
        if (!(check.affected_modes & navigation_mode_category_t::current)) {
            qWarning() << "  " << (int)check.log_level << check.message;
        }
    }
}
