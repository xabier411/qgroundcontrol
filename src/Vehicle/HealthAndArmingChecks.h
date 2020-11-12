/****************************************************************************
 *
 * (c) 2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QVector>

#include <libevents/libs/cpp/parse/parser.h>
#include <libevents/libs/cpp/generated/events_generated.h>

Q_DECLARE_LOGGING_CATEGORY(HealthAndArmingChecks)

class HealthAndArmingCheckHandler : public QObject
{
    Q_OBJECT
public:

    enum class CheckType {
        ArmingCheck,
        Health
    };
    struct Check {
        CheckType type;
        QString message;
        QString description;
        events::common::enums::navigation_mode_category_t affected_modes;
        uint8_t affected_health_component_index; ///< index for events::common::enums::health_component_t, can be 0xff
        events::Log log_level;
    };

    struct HealthSummary {
        events::common::enums::health_component_t is_present;
        events::common::enums::health_component_t error;
        events::common::enums::health_component_t warning;
    };
    struct ArmingCheckSummary {
        events::common::enums::health_component_t error;
        events::common::enums::health_component_t warning;
        events::common::enums::navigation_mode_category_t can_arm;
    };

    struct Results {
        HealthSummary health{};
        ArmingCheckSummary arming{};
        QVector<Check> checks{};

        void reset() {
            health = {};
            arming = {};
            checks.clear();
        }
    };

    void handleEvent(const events::parser::ParsedEvent& event);

    const Results& results() const { return _results[(_current_result + 1) % 2]; }

signals:
    void update();
private:

    enum class Type {
        ArmingCheckSummary,
        Other,
        HealthSummary,
    };

    void reset();

    void testReport();

    Type _expected_event{Type::ArmingCheckSummary};
    Results _results[2]; ///< store the last full set and currently updating one
    int _current_result{0}; ///< index for the currently updating/adding results
};

