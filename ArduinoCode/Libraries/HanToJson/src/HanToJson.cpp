#include "HanToJson.h"
#include "Aidon.h"
#include "Kaifa.h"
#include "Kamstrup.h"


static void hanToJsonKaifa3phase(int listSize, JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    if (listSize >= (int)Kaifa::List3PhaseShort)
    {
        data["lv"]   = hanReader.getString(  (int)Kaifa_List3Phase::ListVersionIdentifier);
        data["id"]   = hanReader.getString(  (int)Kaifa_List3Phase::MeterID);
        data["type"] = hanReader.getString(  (int)Kaifa_List3Phase::MeterType);
        data["P"]    = hanReader.getInt(     (int)Kaifa_List3Phase::ActiveImportPower);
        data["Q"]    = hanReader.getInt(     (int)Kaifa_List3Phase::ReactiveImportPower);
        data["I1"]   = hanReader.getInt(     (int)Kaifa_List3Phase::CurrentL1);
        data["I2"]   = hanReader.getInt(     (int)Kaifa_List3Phase::CurrentL2);
        data["I3"]   = hanReader.getInt(     (int)Kaifa_List3Phase::CurrentL3);
        data["U1"]   = hanReader.getInt(     (int)Kaifa_List3Phase::VoltageL1);
        data["U2"]   = hanReader.getInt(     (int)Kaifa_List3Phase::VoltageL2);
        data["U3"]   = hanReader.getInt(     (int)Kaifa_List3Phase::VoltageL3);
    }

    if (listSize >=  (int)Kaifa::List3PhaseLong)
    {
        data["tPI"]  = hanReader.getInt(     (int)Kaifa_List3Phase::CumulativeActiveImportEnergy);
        data["tPO"]  = hanReader.getInt(     (int)Kaifa_List3Phase::CumulativeActiveExportEnergy);
        data["tQI"]  = hanReader.getInt(     (int)Kaifa_List3Phase::CumulativeReactiveImportEnergy);
        data["tQO"]  = hanReader.getInt(     (int)Kaifa_List3Phase::CumulativeReactiveExportEnergy);
    }
}

static void hanToJsonKaifa1phase(int listSize, JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    if (listSize >=  (int)Kaifa::List1PhaseShort)
    {
        data["lv"]   = hanReader.getString(  (int)Kaifa_List1Phase::ListVersionIdentifier);
        data["id"]   = hanReader.getString(  (int)Kaifa_List1Phase::MeterID);
        data["type"] = hanReader.getString(  (int)Kaifa_List1Phase::MeterType);
        data["P"]    = hanReader.getInt(     (int)Kaifa_List1Phase::ActiveImportPower);
        data["Q"]    = hanReader.getInt(     (int)Kaifa_List1Phase::ReactiveImportPower);
        data["I1"]   = hanReader.getInt(     (int)Kaifa_List1Phase::CurrentL1);
        data["U1"]   = hanReader.getInt(     (int)Kaifa_List1Phase::VoltageL1);
    }

    if (listSize >=  (int)Kaifa::List1PhaseLong)
    {
        data["tPI"]  = hanReader.getInt(     (int)Kaifa_List1Phase::CumulativeActiveImportEnergy);
        data["tPO"]  = hanReader.getInt(     (int)Kaifa_List1Phase::CumulativeActiveExportEnergy);
        data["tQI"]  = hanReader.getInt(     (int)Kaifa_List1Phase::CumulativeReactiveImportEnergy);
        data["tQO"]  = hanReader.getInt(     (int)Kaifa_List1Phase::CumulativeReactiveExportEnergy);
    }
}

static void hanToJsonKaifa(JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    int listSize = hanReader.getListSize();

    if (listSize == (int)Kaifa::List1)
    {
        // Handle listSize == 1 specially
        data["P"]   = hanReader.getInt(     (int)Kaifa_List1::ActivePowerImported);
        return;
    }

    switch (listSize) {
        case (int)Kaifa::List3PhaseShort:
        case (int)Kaifa::List3PhaseLong:
            return hanToJsonKaifa3phase(listSize, data, hanReader, debugger);
        case (int)Kaifa::List1PhaseShort:
        case (int)Kaifa::List1PhaseLong:
            return hanToJsonKaifa1phase(listSize, data, hanReader, debugger);
		default:
			if (debugger) debugger->printf("Warning: Unknown listSize %d\n", listSize);
			return;
    }
}


static void hanToJsonAidon3phase(int listSize, JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    if (listSize >= (int)Aidon::List3PhaseShort)
    {
        data["lv"]      = hanReader.getString(          (int)Aidon_List3Phase::ListVersionIdentifier);
        data["id"]      = hanReader.getString(          (int)Aidon_List3Phase::MeterID);
        data["type"]    = hanReader.getString(          (int)Aidon_List3Phase::MeterType);
        data["P"]       = hanReader.getInt(             (int)Aidon_List3Phase::ActiveImportPower);
        data["Q"]       = hanReader.getInt(             (int)Aidon_List3Phase::ReactiveExportPower);
        data["I1"]      = ((double) hanReader.getInt(   (int)Aidon_List3Phase::CurrentL1));
        data["I2"]      = ((double) hanReader.getInt(   (int)Aidon_List3Phase::CurrentL2));
        data["I3"]      = ((double) hanReader.getInt(   (int)Aidon_List3Phase::CurrentL3));
        data["U1"]      = ((double) hanReader.getInt(   (int)Aidon_List3Phase::VoltageL1));
        data["U2"]      = ((double) hanReader.getInt(   (int)Aidon_List3Phase::VoltageL2));
        data["U3"]      = ((double) hanReader.getInt(   (int)Aidon_List3Phase::VoltageL3));
    }

    if (listSize >= (int)Aidon::List3PhaseShort)
    {
        data["tPI"]     = hanReader.getInt(             (int)Aidon_List3Phase::CumulativeActiveImportEnergy);
        data["tPO"]     = hanReader.getInt(             (int)Aidon_List3Phase::CumulativeActiveExportEnergy);
        data["tQI"]     = hanReader.getInt(             (int)Aidon_List3Phase::CumulativeReactiveImportEnergy);
        data["tQO"]     = hanReader.getInt(             (int)Aidon_List3Phase::CumulativeReactiveExportEnergy);
    }

	// TODO Aidon::List3PhaseLong
}

static void hanToJsonAidon1phase(int listSize, JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    if (listSize >= (int)Aidon::List1PhaseShort)
    {
        data["lv"]      = hanReader.getString(          (int)Aidon_List1Phase::ListVersionIdentifier);
        data["id"]      = hanReader.getString(          (int)Aidon_List1Phase::MeterID);
        data["type"]    = hanReader.getString(          (int)Aidon_List1Phase::MeterType);
        data["P"]       = hanReader.getInt(             (int)Aidon_List1Phase::ActiveImportPower);
        data["Q"]       = hanReader.getInt(             (int)Aidon_List1Phase::ReactiveExportPower);
        data["I1"]      = ((double) hanReader.getInt(   (int)Aidon_List1Phase::CurrentL1));
        data["U1"]      = ((double) hanReader.getInt(   (int)Aidon_List1Phase::VoltageL1));
    }

}

static void hanToJsonAidon(JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    int listSize = hanReader.getListSize();

    // Based on the list number, get all details
    // according to OBIS specifications for the meter
    if (listSize == (int)Aidon::List1)
    {
        // Handle listSize == 1 specially
        data["P"] = hanReader.getInt((int)Aidon_List1::ActiveImportPower);
        return;
    }

    switch (listSize) {
        case (int)Aidon::List3PhaseShort:
        case (int)Aidon::List3PhaseLong:
            return hanToJsonAidon3phase(listSize, data, hanReader, debugger);
        case (int)Aidon::List1PhaseShort:
        case (int)Aidon::List1PhaseLong:
            return hanToJsonAidon1phase(listSize, data, hanReader, debugger);
		default:
			if (debugger) debugger->printf("Warning: Unknown listSize %d\n", listSize);
			return;
    }
}

static void hanToJsonKamstrup3phase(int listSize, JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    if (listSize >= (int)Kamstrup::List3PhaseShort)
    {
        data["lv"]      = hanReader.getString(  (int)Kamstrup_List3Phase::ListVersionIdentifier);
        data["id"]      = hanReader.getString(  (int)Kamstrup_List3Phase::MeterID);
        data["type"]    = hanReader.getString(  (int)Kamstrup_List3Phase::MeterType);
        data["P"]       = hanReader.getInt(     (int)Kamstrup_List3Phase::ActiveImportPower);
        data["Q"]       = hanReader.getInt(     (int)Kamstrup_List3Phase::ReactiveImportPower);
        data["I1"]      = hanReader.getInt(     (int)Kamstrup_List3Phase::CurrentL1);
        data["I2"]      = hanReader.getInt(     (int)Kamstrup_List3Phase::CurrentL2);
        data["I3"]      = hanReader.getInt(     (int)Kamstrup_List3Phase::CurrentL3);
        data["U1"]      = hanReader.getInt(     (int)Kamstrup_List3Phase::VoltageL1);
        data["U2"]      = hanReader.getInt(     (int)Kamstrup_List3Phase::VoltageL2);
        data["U3"]      = hanReader.getInt(     (int)Kamstrup_List3Phase::VoltageL3);
    }

    if (listSize >= (int)Kamstrup::List3PhaseLong)
    {
        data["tPI"]     = hanReader.getInt(     (int)Kamstrup_List3Phase::CumulativeActiveImportEnergy);
        data["tPO"]     = hanReader.getInt(     (int)Kamstrup_List3Phase::CumulativeActiveExportEnergy);
        data["tQI"]     = hanReader.getInt(     (int)Kamstrup_List3Phase::CumulativeReactiveImportEnergy);
        data["tQO"]     = hanReader.getInt(     (int)Kamstrup_List3Phase::CumulativeReactiveExportEnergy);
    }
}

static void hanToJsonKamstrup1phase(int listSize, JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    if (listSize >= (int)Kamstrup::List1PhaseShort)
    {
        data["lv"]      = hanReader.getString(  (int)Kamstrup_List1Phase::ListVersionIdentifier);
        data["id"]      = hanReader.getString(  (int)Kamstrup_List1Phase::MeterID);
        data["type"]    = hanReader.getString(  (int)Kamstrup_List1Phase::MeterType);
        data["P"]       = hanReader.getInt(     (int)Kamstrup_List1Phase::ActiveImportPower);
        data["Q"]       = hanReader.getInt(     (int)Kamstrup_List1Phase::ReactiveImportPower);
        data["I1"]      = hanReader.getInt(     (int)Kamstrup_List1Phase::CurrentL1);
        data["U1"]      = hanReader.getInt(     (int)Kamstrup_List1Phase::VoltageL1);
    }

    if (listSize >= (int)Kamstrup::List1PhaseLong)
    {
        data["tPI"]     = hanReader.getInt(     (int)Kamstrup_List1Phase::CumulativeActiveImportEnergy);
        data["tPO"]     = hanReader.getInt(     (int)Kamstrup_List1Phase::CumulativeActiveExportEnergy);
        data["tQI"]     = hanReader.getInt(     (int)Kamstrup_List1Phase::CumulativeReactiveImportEnergy);
        data["tQO"]     = hanReader.getInt(     (int)Kamstrup_List1Phase::CumulativeReactiveExportEnergy);
    }
}

static void hanToJsonKamstrup(JsonObject& data, HanReader& hanReader, Stream *debugger)
{
    int listSize = hanReader.getListSize();

    switch (listSize) {
        case (int)Kamstrup::List3PhaseShort:
        case (int)Kamstrup::List3PhaseLong:
            return hanToJsonKamstrup3phase(listSize, data, hanReader, debugger);
        case (int)Kamstrup::List1PhaseShort:
        case (int)Kamstrup::List1PhaseLong:
            return hanToJsonKamstrup1phase(listSize, data, hanReader, debugger);
		default:
			if (debugger) debugger->printf("Warning: Unknown listSize %d\n", listSize);
			return;
    }
}

void hanToJson(JsonObject& data, byte meterType, HanReader& hanReader, Stream *debugger)
{
    // Based on the list number, get all details
    // according to OBIS specifications for the meter
    switch (meterType)
    {
        case 1: // Kaifa
            return hanToJsonKaifa(data, hanReader, debugger);
        case 2: // Aidon
            return hanToJsonAidon(data, hanReader, debugger);
        case 3: // Kamstrup
            return hanToJsonKamstrup(data, hanReader, debugger);
        default:
            if (debugger) {
                debugger->print("Meter type ");
                debugger->print(meterType, HEX);
                debugger->println(" is unknown");
            }
            break;
    }
}

void hanToJson(JsonObject& data, byte meterType, HanReader& hanReader)
{
    return hanToJson(data, meterType, hanReader, NULL);
}
