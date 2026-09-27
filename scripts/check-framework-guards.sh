#!/bin/bash

echo "🔍 Running static analysis for framework compatibility..."

errors=0

# Check for unguarded Arduino.h includes
echo "Checking Arduino.h includes..."
if grep -rn "^#include <Arduino.h>" components/dsmr_custom/*.h components/dsmr_custom/*.cpp 2>/dev/null | grep -v "USE_ARDUINO"; then
    echo "❌ ERROR: Found unguarded Arduino.h include!"
    errors=$((errors + 1))
else
    echo "✅ All Arduino.h includes are properly guarded"
fi

# Check for unguarded .concat() calls
echo "Checking .concat() calls..."
if grep -rn "\.concat(" components/dsmr_custom/*.cpp 2>/dev/null | grep -v "#ifdef" | grep -v "//"; then
    echo "⚠️  WARNING: Found potentially unguarded .concat() calls"
fi

# Check for unguarded String += operations in critical files
echo "Checking String += operations in util.h..."
if awk '
    /#ifdef USE_ARDUINO/ { arduino_branch = 1; next }
    /#else/ { arduino_branch = 0; next }
    /#endif/ { arduino_branch = 0; next }
    /error_string[[:space:]]*\+=/ && !arduino_branch { print; failed = 1 }
    END { exit failed }
' components/dsmr_custom/util.h; then
    echo "✅ Arduino String operations are confined to Arduino branches"
else
    echo "❌ ERROR: Found String += outside a USE_ARDUINO branch in util.h!"
    errors=$((errors + 1))
fi

# Check that AUTO_LOAD is present
echo "Checking AUTO_LOAD directive..."
if grep -q "AUTO_LOAD.*sensor.*text_sensor" components/dsmr_custom/__init__.py 2>/dev/null; then
    echo "✅ AUTO_LOAD directive found"
else
    echo "❌ ERROR: AUTO_LOAD directive missing in __init__.py!"
    errors=$((errors + 1))
fi

echo ""
if [ $errors -eq 0 ]; then
    echo "✅ Static analysis passed!"
    exit 0
else
    echo "❌ Static analysis failed with $errors error(s)"
    exit 1
fi
