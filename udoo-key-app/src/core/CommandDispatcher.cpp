#include "CommandDispatcher.h"
#include "DeviceManager.h"
#include "TransactionModel.h"
#include <QJsonDocument>
#include <QJsonArray>

namespace UdooKey {

CommandDispatcher::CommandDispatcher() {
    registerBuiltins();
}

CommandDispatcher& CommandDispatcher::instance() {
    static CommandDispatcher s_instance;
    return s_instance;
}

void CommandDispatcher::registerCommand(const QString& name, const QString& helpText, Handler handler) {
    m_commands[name.toLower()] = CommandEntry{name.toLower(), helpText, handler};
}

void CommandDispatcher::registerBuiltins() {
    registerCommand("help", "List available commands or get help for a specific command",
        [this](const QStringList& args) {
            return help(args.isEmpty() ? QString() : args[0]);
        });

    registerCommand("device.info", "Show connected device hardware model and capabilities",
        [](const QStringList& /*args*/) {
            const auto& caps = DeviceManager::instance().currentCapabilities();
            return caps.summary() + "\nCapabilities: " + caps.capabilityNames().join(", ");
        });

    registerCommand("device.status", "Check connection status",
        [](const QStringList& /*args*/) {
            bool conn = DeviceManager::instance().isConnected();
            return conn ? "Connected" : "Disconnected";
        });

    registerCommand("device.disconnect", "Disconnect active device",
        [](const QStringList& /*args*/) {
            DeviceManager::instance().disconnectDevice();
            return "Device disconnected";
        });

    registerCommand("timeline.count", "Get number of logged transactions",
        [](const QStringList& /*args*/) {
            return QString::number(TransactionLogger::instance().count());
        });

    registerCommand("timeline.clear", "Clear global transaction history",
        [](const QStringList& /*args*/) {
            TransactionLogger::instance().clear();
            return "Transaction history cleared";
        });

    registerCommand("wifi.status", "Show wireless coprocessor status and network telemetry",
        [](const QStringList& /*args*/) {
            if (!DeviceManager::instance().isConnected()) {
                return QString("Instrument disconnected. Wireless coprocessor telemetry unavailable.");
            }
            return QString("ESP32 Coprocessor: Active\nWi-Fi Mode: SoftAP + Station Recon\nAP IP: 192.168.4.1\nStatus: Online");
        });

    registerCommand("wifi.scan", "Trigger ESP32 Wi-Fi environment survey",
        [](const QStringList& /*args*/) {
            auto* trans = DeviceManager::instance().transport();
            if (!trans || !trans->isOpen()) {
                return QString("Error: Device not connected.");
            }
            trans->write("wifi scan\r\n");
            return QString("Initiated Wi-Fi reconnaissance scan on ESP32 coprocessor.");
        });

    registerCommand("bt.scan", "Initiate Bluetooth Low Energy discovery scan",
        [](const QStringList& args) {
            auto* trans = DeviceManager::instance().transport();
            if (!trans || !trans->isOpen()) {
                return QString("Error: Device not connected.");
            }
            int durationSec = args.isEmpty() ? 10 : args[0].toInt();
            if (durationSec <= 0) durationSec = 10;
            QString cmd = QString("bt scan %1\r\n").arg(durationSec);
            trans->write(cmd.toUtf8());
            return QString("Initiated BLE discovery scan (%1s) on ESP32 coprocessor.").arg(durationSec);
        });

    registerCommand("selftest", "Execute dual-MCU hardware self-test and signal path diagnostics",
        [](const QStringList& /*args*/) {
            auto* trans = DeviceManager::instance().transport();
            if (!trans || !trans->isOpen()) {
                return QString("Error: Device not connected.");
            }
            trans->write("selftest\r\n");
            return QString("Initiated hardware self-test diagnostic routine across RP2040 and ESP32.");
        });
}

QString CommandDispatcher::execute(const QString& commandLine) {
    QString trimmed = commandLine.trimmed();
    if (trimmed.isEmpty()) return QString();

    QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
    QString cmdName = parts[0].toLower();
    QStringList args = parts.mid(1);

    auto it = m_commands.find(cmdName);
    if (it != m_commands.end()) {
        try {
            return it->second.handler(args);
        } catch (const std::exception& e) {
            return QString("Error executing '%1': %2").arg(cmdName, e.what());
        } catch (...) {
            return QString("Unknown error executing '%1'").arg(cmdName);
        }
    }

    return QString("Unknown command '%1'. Type 'help' for a list of commands.").arg(cmdName);
}

QJsonObject CommandDispatcher::executeJsonRpc(const QJsonObject& request) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = request["id"];

    QString method = request["method"].toString().toLower();
    auto it = m_commands.find(method);
    if (it == m_commands.end()) {
        QJsonObject err;
        err["code"] = -32601;
        err["message"] = "Method not found";
        response["error"] = err;
        return response;
    }

    QStringList args;
    if (request.contains("params")) {
        QJsonArray params = request["params"].toArray();
        for (const auto& val : params) {
            args.append(val.toString());
        }
    }

    try {
        QString result = it->second.handler(args);
        response["result"] = result;
    } catch (const std::exception& e) {
        QJsonObject err;
        err["code"] = -32000;
        err["message"] = e.what();
        response["error"] = err;
    }

    return response;
}

QStringList CommandDispatcher::availableCommands() const {
    QStringList list;
    for (const auto& pair : m_commands) {
        list.append(pair.second.name);
    }
    return list;
}

QString CommandDispatcher::help(const QString& cmd) const {
    if (!cmd.isEmpty()) {
        auto it = m_commands.find(cmd.toLower());
        if (it != m_commands.end()) {
            return QString("%1: %2").arg(it->second.name, it->second.help);
        }
        return QString("Unknown command '%1'").arg(cmd);
    }

    QString out = "Available Commands:\n";
    for (const auto& pair : m_commands) {
        out += QString("  • %1 — %2\n").arg(pair.second.name, -18).arg(pair.second.help);
    }
    return out;
}

} // namespace UdooKey
