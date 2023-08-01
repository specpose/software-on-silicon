#include <valarray>
#include <iostream>

using SAMPLE_SIZE=float;

template<typename T> class Contiguous {
    public:
    Contiguous(const std::size_t size) : size(size), _storage(new T[size]) {}
    ~Contiguous(){
        //error: double free detected
        /*if (_storage!=nullptr) {
            delete _storage;
            _storage = nullptr;
        }*/
    }
    Contiguous& operator=(const std::vector<T>& other){
        if (other.size()!=size)
            throw std::logic_error("operator=() used incorrectly");
        for (int channel=0;channel<other.size();channel++)
            _storage[channel]=other[channel];
        return *this;
    }
    T& operator[](const std::size_t pos){
        return _storage[pos];
    }
    private:
    T* _storage = nullptr;
    std::size_t size;
};

template<typename T> std::vector<T> item(const SAMPLE_SIZE** buffer, const std::size_t item_size, const std::size_t offset){
    auto result = std::vector<T>(item_size,0.0);
    for(std::size_t channel=0;channel<result.size();channel++){
        result[channel]=(buffer[channel][offset]);
    }
    return result;
}

int main(){
    std::size_t surroundsound = 5;//vst numInputs

    //from source
    const SAMPLE_SIZE channel_even[10]={1.0,0.0,-1.0,0.0,1.0,0.0,-1.0,0.0,1.0,0.0};
    const SAMPLE_SIZE channel_odd[10]={0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0};
    const SAMPLE_SIZE* channel_ptrs[5] = {channel_even,channel_odd,channel_even,channel_odd,channel_even};//vst host assume const
    const SAMPLE_SIZE** channelBuffers32 = static_cast<const SAMPLE_SIZE**>(channel_ptrs);//notconst Sample32(=float) **   channelBuffers32

    //via separate thread
    auto memorycontroller = std::vector<Contiguous<SAMPLE_SIZE>>{};//initial size not provided: no Contiguous default constructor
    std::size_t numSamples = 10;//vst numSamples
    std::size_t count = 0;
    memorycontroller.reserve(numSamples);
    while (memorycontroller.size()<numSamples)
        memorycontroller.push_back(Contiguous<SAMPLE_SIZE>{surroundsound});//beware actualSamplePosition
    while (count<numSamples) {
        const auto entry = item<SAMPLE_SIZE>(channelBuffers32,surroundsound,count);
        memorycontroller[count]=entry;//beware actualSamplePosition: memorycontroller[offset+count]
        count++;
    }

    //deallocating source not needed: Owned by vst
    //error: double free or corruption
    //for (size_t i=0;i<5;i++)
    //    delete channelBuffers32[i];
    //error: called on unallocated object
    //delete channelBuffers32;

    //to target
    //size_t Speczilla::ARAAudioSource::read(void * buffers[], size_t offset, size_t samplesPerChannel)
    size_t samplesPerChannel = 7;
    //API: NOTCONST void* const* buffers: target
    SAMPLE_SIZE** buffers = nullptr;//ARAFallback: NOTCONST void * buffers[]: target
    buffers = new SAMPLE_SIZE*[surroundsound];//OLD: buffers = (void**)malloc(surroundsound*sizeof(void*));
    for (std::size_t i=0;i<surroundsound;i++){
        buffers[i]=new SAMPLE_SIZE[samplesPerChannel];
    }
    for (std::size_t i=0;i<surroundsound;i++){
        for (std::size_t j=0; j<samplesPerChannel;j++)
            buffers[i][j] = memorycontroller[j][i];
    }

    //verify
    size_t row = 4;
    std::cout<<"Verifying 7 entries channel "<<row<<" of ARABuffer:";
    for (std::size_t i=0;i<samplesPerChannel;i++)
        std::cout<< buffers[row][i];
    std::cout<<std::endl;

    //deallocating target: Needed ARAFallback is a host
    if (buffers){
        for (std::size_t i=0;i<surroundsound;i++)
            delete buffers[i];
        delete buffers;
    }
}