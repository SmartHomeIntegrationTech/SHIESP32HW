import sys
from CppHeaderParser import CppHeader, CppParseError, CppVariable

fileStart = '''/*
 * Copyright (c) 2020 Karsten Becker All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

// WARNING, this is an automatically generated file!
// Don't change anything in here.

# include <iostream>
# include <string>'''
fileMiddle = ''' {}

void SHI::ESP32HWConfig::fillData(JsonDocument &doc) {'''
fileEnd = '''}
std::string SHI::ESP32HWConfig::toJson() {
  DynamicJsonDocument doc(capacity);
  fillData(doc);
  char output[2000];
  serializeJson(doc, output, sizeof(output));
  return std::string(output);
}
void SHI::ESP32HWConfig::printJson(std::ostream printer) {
  DynamicJsonDocument doc(capacity);
  fillData(doc);
  serializeJson(doc, printer);
}'''


def generateCodeForProperty(p: CppVariable, initializer: list, toString: list):
    if "default" in p:
        print("   Code for %s=%s" % (p["name"], p["default"]))
        initializer.append("      {}(obj[\"{}\"] | {})".format(
            p["name"], p["name"], p["default"]))
    else:
        print("   Code for %s" % p["name"])
        initializer.append(
            "      {}(obj[\"{}\"])".format(p["name"], p["name"]))
    toString.append("  doc[\"{}\"] = {};".format(p["name"], p["name"]))


def parseHeader(header: str):
    try:
        cppHeader = CppHeader(
            "/Users/karstenbecker/PlatformIO/Projects/SmartHomeIntegration/include/"+header)
    except CppParseError as e:
        print("Ignored {} because it failed to parse: {}".format(header, e))
        return

    initializer = []
    toString = []

    for name, clazz in cppHeader.classes.items():
        print("%s" % name)
        for access, prop in clazz["properties"].items():
            print(" %s" % access)
            if (access == "public"):
                for p in prop:
                    print("  %s" % p)
                    generateCodeForProperty(p, initializer, toString)
            else:
                print("  Skipping non public members")

    cpp = open("cpp.txt", 'w')
    print(fileStart, file=cpp)
    print("#include \"{}\"\nnamespace {{\nconst size_t capacity = JSON_OBJECT_SIZE({});\n}}".format(
        header, len(initializer)), file=cpp)
    print("SHI::ESP32HWConfig::ESP32HWConfig(JsonObject obj):", file=cpp)
    print(",\n".join(initializer), file=cpp)
    print(fileMiddle, file=cpp)
    print("\n".join(toString), file=cpp)
    print(fileEnd, file=cpp)
    cpp.close()


parseHeader("SHIESP32HW_config.h")
