#include <cstdint>
#include <Arduino.h>


struct commandValues
{
  String commandVal;
  String cmdStrArgs[8];
  int cmdNumArgs[8];
  int argsStrPos;
  int argsNumPos;
};

commandValues parseCommand()
{
    commandValues parseCommand;
    if (Serial.available())
    {
        String commandInput(Serial.readStringUntil('\n'));
        commandInput.trim();
        commandInput.toLowerCase();

        parseCommand.commandVal = commandInput.substring(0, commandInput.indexOf(' '));
        Serial.printf("%s <- command itself\n", parseCommand.commandVal.c_str());
        String commandArgs = commandInput.substring(commandInput.indexOf(' '));
        commandArgs.trim();
        commandArgs.toLowerCase(); // ags string with spaces
        Serial.printf("%s <- all args string with spaces\n", commandArgs.c_str());
        Serial.printf("%d <- comm args string length\n", commandArgs.length());

        String argsString[16]; // maybe into the structure right away? no
        int argsCount = 0;

        for (int i = 0; i <= commandArgs.length(); i++)
        {
            if (commandArgs[i] == ' ' && commandArgs[i + 1] != ' ')
            {
                argsCount++;
            }
            argsString[argsCount] += commandArgs[i];
        }

        Serial.printf("%d <- args count\n", argsCount);

        parseCommand.argsNumPos = 0;
        parseCommand.argsStrPos = 0;

        for (int k = 0; k <= argsCount; k++)
        {
            argsString[k].trim();
            Serial.printf("%s <- arg num %d\n", argsString[k].c_str(), k);
            if ((int)argsString[k][0] > 47 && (int)argsString[k][0] < 58)
            {
                parseCommand.cmdNumArgs[parseCommand.argsNumPos] = argsString[k].toInt();
                parseCommand.argsNumPos++;
            }
            else
            {
                parseCommand.cmdStrArgs[parseCommand.argsStrPos] = argsString[k];
                parseCommand.argsStrPos++;
            }
        }

        Serial.printf("%d <- qty Str vals\n", parseCommand.argsStrPos);
        Serial.printf("%d <- qty Num vals\n", parseCommand.argsNumPos);

        for (int i = 0; i < parseCommand.argsStrPos; i++)
        {
            Serial.printf("%s <- struct Str val num %d\n", parseCommand.cmdStrArgs[i].c_str(), i);
        }

        for (int i = 0; i < parseCommand.argsNumPos; i++)
        {
            Serial.printf("%d <- struct Num val num %d\n", parseCommand.cmdNumArgs[i], i);
        }

        Serial.printf("\n========== Done ============\n");
    }
    return parseCommand;
}