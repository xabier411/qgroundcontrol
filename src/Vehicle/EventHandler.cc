/****************************************************************************
 *
 * (c) 2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include <QSharedPointer>

#include "EventHandler.h"
#include "QGCLoggingCategory.h"

Q_DECLARE_METATYPE(QSharedPointer<events::parser::ParsedEvent>);
QGC_LOGGING_CATEGORY(Events, "Events");


EventHandler::EventHandler(QObject* parent, const QString& profile, handle_event_f handle_event_cb,
            send_request_event_message_f send_request_cb,
            uint8_t our_system_id, uint8_t our_component_id, uint8_t system_id, uint8_t component_id)
    : QObject(parent), _timer(parent),
    _handle_event_cb(handle_event_cb),
    _send_request_cb(send_request_cb),
    _compid(component_id)
{
    auto error_cb = [component_id](int num_events_lost) {
        qCWarning(Events) << "Events got lost:" << num_events_lost << "comp_id:" << component_id;
    };

    auto timeout_cb = [this](int timeout_ms) {
        if (timeout_ms < 0) {
            _timer.stop();
        } else {
            _timer.setSingleShot(true);
            _timer.start(timeout_ms);
        }
    };

    _parser.setProfile(profile.toStdString());

    _parser.formatters().url = [](const std::string& content, const std::string& link) {
        return "<a href=\""+link+"\">"+content+"</a>"; };

    events::ReceiveProtocol::Callbacks callbacks{error_cb, _send_request_cb,
        std::bind(&EventHandler::gotEvent, this, std::placeholders::_1), timeout_cb};
    _protocol = new events::ReceiveProtocol(callbacks, our_system_id, our_component_id, system_id, component_id);

    connect(&_timer, &QTimer::timeout, this, [this]() { _protocol->timerEvent(); });

    qRegisterMetaType<QSharedPointer<events::parser::ParsedEvent>>("ParsedEvent");
}

EventHandler::~EventHandler()
{
    delete _protocol;
}

void EventHandler::gotEvent(const mavlink_event_t& event)
{
    if (!_parser.hasDefinitions()) {
        if (_pending_events.size() > 50) { // limit size (not expected to happen)
            _pending_events.clear();
        }
        qCDebug(Events) << "No metadata, queuing event, ID:" << event.id << "num pending:" << _pending_events.size();
        _pending_events.push_back(event);
        return;
    }

    std::unique_ptr<events::parser::ParsedEvent> parsed_event = _parser.parse(event);
    if (parsed_event == nullptr) {
        qCWarning(Events) << "Got Event w/o known metadata: ID:" << event.id << "comp id:" << _compid;
        return;
    }

    qCDebug(Events) << "Got Event: ID:" << parsed_event->id() << "namespace:" << parsed_event->eventNamespace().c_str() <<
            "name:" << parsed_event->name().c_str() << "msg:" << parsed_event->message().c_str();

    _handle_event_cb(std::move(parsed_event));
}

void EventHandler::handleEvents(const mavlink_message_t& message)
{
    _protocol->processMessage(message);
}

void EventHandler::setMetadata(const QString &metadataJsonFileName, const QString &translationJsonFileName)
{
    auto translate = [](const std::string& s) {
        // TODO: use translation file
        return s;
    };
    if (_parser.loadDefinitionsFile(metadataJsonFileName.toStdString(), translate)) {
        if (_parser.hasDefinitions()) {
            // do we have queued events?
            for (const auto& event : _pending_events) {
                gotEvent(event);
            }
            _pending_events.clear();
        }
    } else {
        qCWarning(Events) << "Failed to load events JSON metadata file";
    }
}

