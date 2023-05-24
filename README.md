# Energiewürfel

## Masterarbeit Smart-Home Batteriespeicher für Balkonkraftwerke

- ### Architektur

    Es wird ein Kombiboard aus einem Arduino Uno und einem ESP8266 benutzt, welche über UART Befehle und Messdaten austauschen.

    - [Arduino (Arduino_Code)](https://github.com/PaulusElektrus/EnergyCube-Arduino)

        Der Arduino übernimmt die Steuerung der Hardware

    - [ESP8266 (ESP_code)](https://github.com/PaulusElektrus/EnergyCube-ESP)

        Der ESP8266 mit seinem W-LAN Modul übernimmt die Kommunikation mit dem Internet

- ### Kommunikation

    Die Kommunikation zwischen den Mikrocontrollern erfolgt über UART. 

    Ein Screenshot des Logic Analyzers welchen ich zur Überwachung der Kommunikation benutze:

    ![Screenshot](Arduino_ESP_Communication.png)

    Man erkennt in den < > geschweiften Klammern die jeweils übertragene Nachricht und danach als Empfangsbestätigung (auch als Debug und zu Demonstrationszwecken verwendet) die Ausgabe des jeweiligen Mikrocontrollers im richtigen Datenformat mit Vorzeichen.

- ### Weitere Informationen folgen...