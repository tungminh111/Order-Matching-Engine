add_library(
    matching_core 
    StreamConsumer.cpp
    L2DataPublisher.cpp
    OrderMatcher.cpp
)

target_include_directories(matching_core PUBLIC ../../include/)
