#include "Skybox.h"
#include "pktx/texture.h"
#include "Pistachio/Utils/PlatformUtils.h"
namespace Pistachio
{
    Result<Skybox*> Skybox::Create(std::string_view path)
    {
        auto ret = std::make_unique<Skybox>();
        auto err = ret->Initialize(path);
        if(!err.Successful()) return ezr::err(std::move(err));
        return ret.release();
    }
    Result<Skybox*> Skybox::Create(const void* memory, size_t size)
    {
        auto ret = std::make_unique<Skybox>();
        auto err = ret->Initialize(memory, size);
        if(!err.Successful()) return ezr::err(std::move(err));
        return ret.release();
    }
    /*
        pSkb Specification (all numbers are little endian)
        - First 4 bytes size of base skybox (sb)
        - Next 4 bytes size of ir skybox (si)
        - Next 4 bytes size of pf skybox (sp)
        - Next (sb) bytes a ktx/2 texture containing the base skybox
          This texture must:
          - Be a cubemap with 1 mip level, 1 array layer and 6 faces
          - Format should be preferrably a regular common color format or one that is transcode-able from
        - Next (si) bytes a ktx/2 texture containing the irradiance skybox
          This texture must satisfy all the constraints of the base skybox
        - Next (sp) bytes a ktx/2 texture containing the prefilter skybox
          This texture must:
          - Be a cubemap with 1-10 miplevels (you can do more, but 10 is recommended), 1 array layer and 6 faces
          - Format constraints remain the same
        Note that memory passed in is expected ot be little endian also
    */
    
    Error validate_base_ir_texture_structure(const pktx::Texture& tex)
    {
        using enum ErrorType;
        if(tex.NumLevels() != 1) return Error(InvalidFile, "More than one mip level on base cubemap");
        if(tex.NumLayers() != 1) return Error(InvalidFile, "More than 1 array layer on base cubemap");
        if(!tex.IsCubemap()) return Error(InvalidFile, "Base texture is supposed to be a cubemap");
        return Error(Success);
    }
    Error validate_pf_texture_structure(const pktx::Texture& tex)
    {
        using enum ErrorType;
        if(tex.NumLevels() > 10) 
        {
            PT_CORE_WARN("Skybox prefilter has more than 10 levels, this is overkill");
        }
        if(tex.NumLayers() != 1) return Error(InvalidFile, "More than 1 array layer on base cubemap");
        if(!tex.IsCubemap()) return Error(InvalidFile, "Base texture is supposed to be a cubemap");
        return Error(Success);
    }
    Result<pktx::Texture> ValidateBaseOrIRTexture(ezr::result<pktx::Texture, ktx_error_code_e>&& base_result)
    {
        if(base_result.is_err()) return ezr::err(Error::FromKTXError(base_result.err()));
        pktx::Texture base = std::move(base_result).value();
        auto e = validate_base_ir_texture_structure(base);
        if(!e.Successful()) return ezr::err(e);
        return base;
    }
    Result<pktx::Texture> ValidatePFTexture(ezr::result<pktx::Texture, ktx_error_code_e>&& pf_result)
    {
        if(pf_result.is_err()) return ezr::err(Error::FromKTXError(pf_result.err()));
        pktx::Texture pf = std::move(pf_result).value();
        auto e = validate_pf_texture_structure(pf);
        if(!e.Successful()) return ezr::err(e);
        return pf;
    }

    Result<pktx::Texture> BaseOrIRTextureFromFile(std::ifstream& file)
    {
        ktxStream s;
        auto pos = file.tellg();
        pktx::ktxStreamFromFile(file, &s, pos);
        return ValidateBaseOrIRTexture(pktx::Texture::CreateFromStream(&s, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT));
    }
    Result<pktx::Texture> BaseOrIRTextureFromMemory(const void* mem, size_t size)
    {
        return ValidateBaseOrIRTexture(pktx::Texture::CreateFromMemory((uint8_t*)mem, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT));
    }
    Result<pktx::Texture> PFTextureFromFile(std::ifstream& file)
    {
        ktxStream s;
        auto pos = file.tellg();
        pktx::ktxStreamFromFile(file, &s, pos);
        return ValidatePFTexture(pktx::Texture::CreateFromStream(&s, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT));
    }
    Result<pktx::Texture> PFTextureFromMemory(const void* mem, size_t size)
    {
        return ValidatePFTexture(pktx::Texture::CreateFromMemory((uint8_t*)mem, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT));
    }
    Error Skybox::InitializeCubeMaps(pktx::Texture& base_ktx, pktx::Texture& ir_ktx, pktx::Texture& pf_ktx)
    {
        auto e = CubeMap::Create(base_ktx).handle(
            [this](auto&& cm){
                base = cm;
                return Error(ErrorType::Success);
            },
            [](auto&& err) { return err; }
        );
        if(!e.Successful()) return e;


        e = CubeMap::Create(ir_ktx).handle(
            [this](auto&& cm){
                ir = cm;
                return Error(ErrorType::Success);
            },
            [](auto&& err) { return err; }
        );
        if(!e.Successful()) 
        {
            delete base; return e;
        }

        e = CubeMap::Create(base_ktx).handle(
            [this](auto&& cm){
                base = cm;
                return Error(ErrorType::Success);
            },
            [](auto&& err) { return err; }
        );
        if(!e.Successful())
        {
            delete base; delete ir; return e;
        }
        return Error(ErrorType::Success);
    }
    Error Skybox::Initialize(std::string_view path)
    {
        std::ifstream file(std::string{path}, std::ios::binary);
        uint32_t sb, si, sp;
        file.read((char*)&sb, sizeof sb);
        file.read((char*)&si, sizeof si);
        file.read((char*)&sp, sizeof sp);
        sb = Edian::ConvertToSystemEndian(sb, Pistachio::Little);
        si = Edian::ConvertToSystemEndian(si, Pistachio::Little);
        sp = Edian::ConvertToSystemEndian(sp, Pistachio::Little);
        
        auto base_res = BaseOrIRTextureFromFile(file);
        if(base_res.is_err()) return base_res.err();
        auto base_ktx = std::move(base_res).value();

        file.seekg(sizeof(uint32_t) * 3 + sb, std::ios::beg); //start of irradiance
        auto ir_res = BaseOrIRTextureFromFile(file);
        if(ir_res.is_err()) return ir_res.err();
        auto ir_ktx = std::move(ir_res).value();
        
        file.seekg(sizeof(uint32_t) * 3 + sb + si, std::ios::beg);
        auto pf_res = PFTextureFromFile(file);
        if(pf_res.is_err()) return pf_res.err();
        auto pf_ktx = std::move(pf_res).value();

        return InitializeCubeMaps(base_ktx, ir_ktx, pf_ktx);
    }
    Error Skybox::Initialize(const void* memory, size_t size)
    {
        if(size < (sizeof(uint32_t) * 3)) return Error(ErrorType::ParameterError, "Provided memory is too small to be a valid skybox");
        uint32_t sb = Edian::ConvertToSystemEndian(((uint32_t*)memory)[0], Pistachio::Little);
        uint32_t si = Edian::ConvertToSystemEndian(((uint32_t*)memory)[1], Pistachio::Little);
        uint32_t sp = Edian::ConvertToSystemEndian(((uint32_t*)memory)[2], Pistachio::Little);

        size_t mem_offset = sizeof(uint32_t) * 3;

        if(size < (sizeof(uint32_t) * 3 + sb + si + sp)) return Error(ErrorType::ParameterError, "Provided memory is too small to be a valid skybox");
        auto base_res = BaseOrIRTextureFromMemory((uint8_t*)memory + mem_offset, sb);
        if(base_res.is_err()) return base_res.err();
        auto base_ktx = std::move(base_res).value();

        mem_offset += sb;
        auto ir_res = BaseOrIRTextureFromMemory((uint8_t*)memory + mem_offset, si);
        if(ir_res.is_err()) return ir_res.err();
        auto ir_ktx = std::move(ir_res).value();

        mem_offset += si;
        auto pf_res = PFTextureFromMemory((uint8_t*)memory + mem_offset, sp);
        if(pf_res.is_err()) return pf_res.err();
        auto pf_ktx = std::move(pf_res).value();

        return InitializeCubeMaps(base_ktx, ir_ktx, pf_ktx);
    }
    Skybox::~Skybox()
    {
        delete base;
        delete ir;
        delete pf;
    }
}