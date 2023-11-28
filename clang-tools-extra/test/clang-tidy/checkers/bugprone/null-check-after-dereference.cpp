// RUN: %check_clang_tidy %s bugprone-null-check-after-dereference %t

struct S {
  int a;
};

int warning_deref(int *p) {
  *p = 42;

  if (p) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point [bugprone-null-check-after-dereference]
    // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
  // FIXME: If there's a direct path, make the error message more precise, ie. remove `one of the locations`
    *p += 20;
    return *p;
  } else {
    return 0;
  }
}

int warning_member(S *q) {
  q->a = 42;

  if (q) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
    q->a += 20;
    return q->a;
  } else {
    return 0;
  }
}

int negative_warning(int *p) {
  *p = 42;

  if (!p) {
    // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
    return 0;
  } else {
    *p += 20;
    return *p;
  }
}

int no_warning(int *p, bool b) {
  if (b) {
    *p = 42;
  }

  if (p) {
    // CHECK-MESSAGES-NOT: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point 
    *p += 20;
    return *p;
  } else {
    return 0;
  }
}

int else_branch_warning(int *p, bool b) {
  if (b) {
    *p = 42;
  } else {
    *p = 20;
  }

  if (p) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-7]]:5: note: one of the locations where the pointer's value cannot be null
    return 0;
  } else {
    *p += 20;
    return *p;
  }
}

int two_branches_warning(int *p, bool b) {
  if (b) {
    *p = 42;
  }
  
  if (!b) {
    *p = 20;
  }

  if (p) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-9]]:5: note: one of the locations where the pointer's value cannot be null
    return 0;
  } else {
    *p += 20;
    return *p;
  }
}

int two_branches_reversed(int *p, bool b) {
  if (!b) {
    *p = 42;
  }
  
  if (b) {
    *p = 20;
  }

  if (p) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-9]]:5: note: one of the locations where the pointer's value cannot be null
    return 0;
  } else {
    *p += 20;
    return *p;
  }
}


int regular_assignment(int *p, int *q) {
  *p = 42;
  q = p;

  if (q) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-5]]:3: note: one of the locations where the pointer's value cannot be null
    *p += 20; 
    return *p;
  } else {
    return 0;
  }
}

int nullptr_assignment(int *nullptr_param, bool b) {
  *nullptr_param = 42;
  int *nullptr_assigned;

  if (b) {
    nullptr_assigned = nullptr;
  } else {
    nullptr_assigned = nullptr_param;
  }

  if (nullptr_assigned) {
    // CHECK-MESSAGES-NOT: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    *nullptr_assigned = 20;
    return *nullptr_assigned;
  } else {
    return 0;
  }
}

extern int *fncall();
extern void refresh_ref(int *&ptr);
extern void refresh_ptr(int **ptr);

int fncall_reassignment(int *fncall_reassigned) {
  *fncall_reassigned = 42;

  fncall_reassigned = fncall();

  if (fncall_reassigned) {
    // CHECK-MESSAGES-NOT: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    *fncall_reassigned = 42;
  }
  
  fncall_reassigned = fncall();

  *fncall_reassigned = 42;

  if (fncall_reassigned) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
    *fncall_reassigned = 42;
  }
  
  refresh_ptr(&fncall_reassigned);

  if (fncall_reassigned) {
    // CHECK-MESSAGES-NOT: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    *fncall_reassigned = 42;
  }
  
  refresh_ptr(&fncall_reassigned);
  *fncall_reassigned = 42;

  if (fncall_reassigned) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
    *fncall_reassigned = 42;
    return *fncall_reassigned;
  } else {
    return 0;
  }
}

int chained_references(int *a, int *b, int *c, int *d, int *e) {
  *a = 42;

  if (a) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
    *b = 42;
  }

  if (b) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-5]]:5: note: one of the locations where the pointer's value cannot be null
    *c = 42;
  }

  if (c) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-5]]:5: note: one of the locations where the pointer's value cannot be null
    *d = 42;
  }

  if (d) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-5]]:5: note: one of the locations where the pointer's value cannot be null
    *e = 42;
  }

  if (e) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-5]]:5: note: one of the locations where the pointer's value cannot be null
    return *a;
  } else {
    return 0;
  }
}

int chained_if(int *a) {
  if (!a) {
    return 0;
  }

  // FIXME: Negations are not tracked properly when the previous conditional returns
  if (a) {
    // --CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    *a += 20;
    return *a;
  } else {
    return 0;
  }
}

int double_if(int *a) {
  if (a) {
    if (a) {
      // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: pointer value is checked even though it cannot be null at this point
      // --CHECK-MESSAGES: :[[@LINE-3]]:5: note: one of the locations where the pointer's value cannot be null
      // FIXME: Add warning for branch statements where pointer is not null afterwards
      *a += 20;
      return *a;
    } else {
      return 0;
    }
  }

  return 0;
}

int while_loop(int *p, volatile bool *b) {
  while (true) {
    if (*b) {
      *p = 42;
      break;
    }
  }

  if (p) {
    // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: pointer value is checked even though it cannot be null at this point
    // CHECK-MESSAGES: :[[@LINE-7]]:7: note: one of the locations where the pointer's value cannot be null
    *p = 42;
    return *p;
  } else {
    return 0;
  }
}

int ternary_op(int *p, int k) {
  *p = 42;

  return p ? *p : k;
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: pointer value is checked even though it cannot be null at this point
  // CHECK-MESSAGES: :[[@LINE-4]]:3: note: one of the locations where the pointer's value cannot be null
}

// In an earlier version, the check would crash on C++17 structured bindings.
int cxx17_crash(int *p) {
  *p = 42;

  int arr[2] = {1, 2};
  auto [a, b] = arr;
  
  return 0;
}
