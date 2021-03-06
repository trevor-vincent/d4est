echo "Starting Two Punctures Regression test"
echo ${PWD}
sed 's|num_of_amr_steps = .*|num_of_amr_steps = 0|g' ../TwoPunctures/options_test.input > two_punctures_options.input
../TwoPunctures/two_punctures_robin_diff_geom_estimator_global_driver two_punctures_options.input > disco4est.out
RES=$(cat disco4est.out | grep -c "56.0000000000000000 0.00042501315")
rm two_punctures_options.input
if [ $RES -ne "1" ]; then
    echo "Test Fail"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT BEGIN *****************"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT BEGIN *****************"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT BEGIN *****************"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT BEGIN *****************"
    cat disco4est.out
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT END *****************"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT END *****************"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT END *****************"
    echo "****************** TWO_PUNCTURES REGRESSION TEST OUTPUT END *****************"
    rm disco4est.out
    exit 1
else
    echo "Test Success"
    rm disco4est.out
fi



