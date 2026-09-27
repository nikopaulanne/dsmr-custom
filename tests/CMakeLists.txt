cmake_minimum_required(VERSION 3.20)
project(dsmr_custom_host_tests LANGUAGES C CXX)

find_package(OpenSSL 1.1 REQUIRED COMPONENTS Crypto)

enable_testing()
foreach(idf_major IN ITEMS 5 6)
  set(test_target crypto_wrapper_test_idf${idf_major})
  if(idf_major EQUAL 6)
    set(crypto_host_backend host/psa_crypto_host.cpp)
  else()
    set(crypto_host_backend host/esp_aes_gcm_host.cpp)
  endif()
  add_executable(${test_target}
    crypto_wrapper_test.cpp
    ${crypto_host_backend}
    ../components/dsmr_custom/dsmr_crypto_impl.cpp
  )
  target_compile_features(${test_target} PRIVATE cxx_std_11)
  target_include_directories(${test_target} PRIVATE
    host
    ../components/dsmr_custom
  )
  target_compile_definitions(${test_target} PRIVATE
    USE_ESP_IDF
    ESP_IDF_VERSION_MAJOR=${idf_major}
  )
  target_link_libraries(${test_target} PRIVATE OpenSSL::Crypto)
  add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()

add_executable(crypto_helpers_test crypto_helpers_test.cpp)
target_compile_features(crypto_helpers_test PRIVATE cxx_std_11)
target_include_directories(crypto_helpers_test PRIVATE
  ../components/dsmr_custom
)
add_test(NAME crypto_helpers_test COMMAND crypto_helpers_test)
