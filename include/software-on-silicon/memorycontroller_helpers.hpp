namespace SFA {
    template<typename InputContainerType> struct DeductionGuide {
        using input_container_type = InputContainerType;
    };
    //template<typename DG> struct deduction_guide : DeductionGuide<typename DG::input_container_type> {
    //    //using input_container_type = typename DG::input_container_type;
    //};
}