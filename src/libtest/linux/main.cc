//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <iostream>


__attribute__((constructor))
void injection_test()
{
    std::println(std::cout,"Libtest up and running!");
}