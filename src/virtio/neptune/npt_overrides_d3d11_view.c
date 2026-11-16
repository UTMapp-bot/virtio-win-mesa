/*
 * Copyright 2026 Turing Software LLC
 * SPDX-License-Identifier: MIT
 *
 * ID3D11View::GetResource for every view family.
 *
 * Apps cast GetResource's answer straight to ID3D11Texture2D (or Buffer,
 * ...) without QueryInterface: on Microsoft's runtime the resource object
 * always carries its most-derived vtable.  The canonical reply hands back
 * the wrapper the guest holds for the resource when there is one, but once
 * the app has released the resource and kept only a view (or got the view
 * back from OMGetRenderTargets and friends), the host registers the answer
 * under the slot's declared type, ID3D11Resource, and the guest builds a
 * base-interface wrapper whose vtable ends before GetDesc.  Resolve such an
 * answer to the concrete interface through QueryInterface, which the host
 * registers as its own id, and memoize it on the view so every call returns
 * the same pointer.
 */

#include "npt_com.h"
#include "npt_overrides.h"
#include "npt_resource.h"

#include "neptune-protocol/npt_protocol_client_id3d11depthstencilview.h"
#include "neptune-protocol/npt_protocol_client_id3d11rendertargetview.h"
#include "neptune-protocol/npt_protocol_client_id3d11resource.h"
#include "neptune-protocol/npt_protocol_client_id3d11shaderresourceview.h"
#include "neptune-protocol/npt_protocol_client_id3d11unorderedaccessview.h"
#include "neptune-protocol/npt_protocol_client_id3d11videodecoderoutputview.h"
#include "neptune-protocol/npt_protocol_client_id3d11videoprocessorinputview.h"
#include "neptune-protocol/npt_protocol_client_id3d11videoprocessoroutputview.h"
#include "neptune-protocol/npt_protocol_client_id3d11view.h"
#include "neptune-protocol/npt_protocol_defs.h"

/* The view's memo key for the concrete answer; the generated thunk keeps
 * the raw answer under the GetResource key. */
#define VIEW_CONCRETE_RESOURCE_MEMO \
   NPT_CMD_TYPE(255, NPT_IFACE_ID_ID3D11Resource, \
                NPT_METHOD_ID3D11Resource_GetType)

static const GUID *
view_resource_iid(D3D11_RESOURCE_DIMENSION dim)
{
   switch (dim) {
   case D3D11_RESOURCE_DIMENSION_BUFFER:    return &NPT_IID_ID3D11Buffer;
   case D3D11_RESOURCE_DIMENSION_TEXTURE1D: return &NPT_IID_ID3D11Texture1D;
   case D3D11_RESOURCE_DIMENSION_TEXTURE2D: return &NPT_IID_ID3D11Texture2D;
   case D3D11_RESOURCE_DIMENSION_TEXTURE3D: return &NPT_IID_ID3D11Texture3D;
   default:                                 return NULL;
   }
}

static void NPT_STDMETHODCALLTYPE
view_GetResource_override(void *self, ID3D11Resource **ppResource)
{
   if (!ppResource)
      return;
   *ppResource = NULL;

   void *memo = npt_com_memo_lookup(self, VIEW_CONCRETE_RESOURCE_MEMO,
                                    &NPT_IID_ID3D11Resource);
   if (memo) {
      *ppResource = memo;
      return;
   }

   ID3D11Resource *res = NULL;
   npt_id3d11view_default_GetResource(self, &res);
   if (!res)
      return;
   *ppResource = res;

   /* The wrapper the resource was created as, or one built from a
    * concrete registration: already the right vtable. */
   if (npt_d3d11_texture_cast(res) || npt_d3d11_buffer_cast(res))
      return;
   const struct npt_com_base *com = (const struct npt_com_base *)res;
   if (com->lpVtbl != (const void **)&npt_id3d11resource_default_vtbl_storage)
      return;

   const struct npt_id3d11resource_client_vtbl *rv =
      (const struct npt_id3d11resource_client_vtbl *)com->lpVtbl;
   D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
   rv->GetType(res, &dim);
   const GUID *iid = view_resource_iid(dim);
   if (!iid)
      return;

   /* Keep the base wrapper rather than answer NULL, or answer another
    * base-interface wrapper. */
   void *typed = NULL;
   if (NPT_FAILED(rv->QueryInterface(res, iid, &typed)) || !typed)
      return;
   if (!npt_d3d11_texture_cast(typed) && !npt_d3d11_buffer_cast(typed)) {
      ((const struct npt_id3d11resource_client_vtbl *)
          ((struct npt_com_base *)typed)->lpVtbl)->Release(typed);
      return;
   }

   npt_com_memo_store(self, VIEW_CONCRETE_RESOURCE_MEMO,
                      &NPT_IID_ID3D11Resource, typed);
   rv->Release(res);
   *ppResource = typed;
}

void
npt_overrides_d3d11_view_init(void)
{
   NPT_REGISTER_OVERRIDE(id3d11view, GetResource, view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11shaderresourceview, GetResource,
                         view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11rendertargetview, GetResource,
                         view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11depthstencilview, GetResource,
                         view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11unorderedaccessview, GetResource,
                         view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11videodecoderoutputview, GetResource,
                         view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11videoprocessorinputview, GetResource,
                         view_GetResource_override);
   NPT_REGISTER_OVERRIDE(id3d11videoprocessoroutputview, GetResource,
                         view_GetResource_override);
}
