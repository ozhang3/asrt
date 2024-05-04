#ifndef CF9AD91A_4391_4E16_A94C_045F54404E4A
#define CF9AD91A_4391_4E16_A94C_045F54404E4A


#include <concepts>
#include "asrt/type_traits.hpp"


template <typename Protocol>
concept InternetDomain = ProtocolTraits::is_internet_domain<Protocol>::value; 

template <typename Protocol>
concept StreamBased = ProtocolTraits::is_stream_based<Protocol>::value;

#endif /* CF9AD91A_4391_4E16_A94C_045F54404E4A */
