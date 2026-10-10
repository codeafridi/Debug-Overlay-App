#include "KdeFocusState.h"

KdeFocusState &kdeFocusState() {
    static KdeFocusState state;
    return state;
}
