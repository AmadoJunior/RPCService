#include <gtest/gtest.h>
#include "Calculator.h"

class CalculatorTest : public testing::Test {
protected:
    Calculator calc;
};

TEST_F(CalculatorTest, Addition) {
    EXPECT_EQ(calc.add(2, 3), 5);
    EXPECT_EQ(calc.add(-1, 1), 0);
    EXPECT_EQ(calc.add(0, 0), 0);
}

TEST_F(CalculatorTest, Subtraction) {
    EXPECT_EQ(calc.subtract(5, 3), 2);
    EXPECT_EQ(calc.subtract(1, 1), 0);
    EXPECT_EQ(calc.subtract(0, 5), -5);
}

//int main(int argc, char** argv) {
//    testing::InitGoogleTest(&argc, argv);
//    return RUN_ALL_TESTS();
//}