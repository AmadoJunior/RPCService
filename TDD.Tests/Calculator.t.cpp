#include "pch.h"
#include <gtest/gtest.h>
#include "Calculator.h"

namespace SampleTests {
    TEST(Calculator, Addition) {
        Calculator calc;
        EXPECT_EQ(calc.add(2, 3), 5);
    }
}