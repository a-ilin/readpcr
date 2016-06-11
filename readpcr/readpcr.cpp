#include "stdafx.h"

#pragma comment(lib, "Tbs.lib")

void logResult(TBS_RESULT result, const char* msg)
{
    if ( result != TBS_SUCCESS )
    {
        std::cout << msg << " Error code: " << result << std::endl;
    }
}

struct ContextDeleter
{
    void operator() (void* c)
    {
        if ( c != nullptr )
        {
            TBS_RESULT result = Tbsip_Context_Close((TBS_HCONTEXT)c);
            logResult(result, "Cannot close context.");
        }
    }
};

inline uint16_t SwapEndian(uint16_t val)
{
    return (val << 8) | (val >> 8);
}

inline uint32_t SwapEndian(uint32_t val)
{
    return (val << 24) | ((val << 8) & 0x00ff0000) |
        ((val >> 8) & 0x0000ff00) | (val >> 24);
}

template <typename N>
class BE_uint
{
public:
    BE_uint()
        : m_value(0)
    {}

    BE_uint(N value)
        : m_value(SwapEndian(value))
    {}

    BE_uint<N>& operator= (N value) { m_value = SwapEndian(value); return *this; }

    operator N () const { return SwapEndian(m_value); }

    N be_value() const { return m_value; }
    N le_value() const { return SwapEndian(m_value); }

private:
    N m_value;
};

int main()
{
    TBS_RESULT result;
    
    TBS_HCONTEXT hContext;
    TBS_CONTEXT_PARAMS contextParams;
    contextParams.version = TBS_CONTEXT_VERSION_ONE;
    
    result = Tbsi_Context_Create(&contextParams, &hContext);
    logResult(result, "Cannot create context.");
    if ( result != TBS_SUCCESS )
    {
        return -1;
    }

    std::unique_ptr<void, ContextDeleter> guardContext(hContext);

#pragma pack(push,1)
    struct
    {
        BE_uint<uint16_t> tag_rqu;
        BE_uint<uint32_t> size;
        BE_uint<uint32_t> command_code;
        BE_uint<uint32_t> pcr_index;
    } ReadPcrCommand;

    struct
    {
        BE_uint<uint16_t> tag_rsp;
        BE_uint<uint32_t> size;
        BE_uint<uint32_t> result_code;
        BYTE pcr_value[20];
    } ReadPcrReply;
#pragma pack(pop)

    ReadPcrCommand.tag_rqu = 0xC1;
    ReadPcrCommand.size = sizeof(ReadPcrCommand);
    ReadPcrCommand.command_code = 21;

    for ( int i = 0; i < 24; ++i )
    {
        ReadPcrCommand.pcr_index = i;

        uint32_t replySize = sizeof(ReadPcrReply);

        result = Tbsip_Submit_Command(hContext, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL,
            (PCBYTE)&ReadPcrCommand, sizeof(ReadPcrCommand),
                                      (PBYTE)&ReadPcrReply, &replySize);
        logResult(result, std::string(std::string("Error reading PCR register #") + std::to_string(i)).c_str());
        
        if ( result == TBS_SUCCESS )
        {
            char num[10];
            snprintf(num, sizeof(num), "%02d", i);
            std::cout << "PCR" << num << ' ';

            for ( int k = 0; k < sizeof(ReadPcrReply.pcr_value); ++k )
            {
                snprintf(num, sizeof(num), "%02X", ReadPcrReply.pcr_value[k]);
                std::cout << ' ' << num;
            }

            std::cout << std::endl;
        }
    }

    return 0;
}

