package com.arbiter.mobile

import android.app.PendingIntent
import android.content.Intent
import android.net.Uri
import android.os.CancellationSignal
import android.os.OutcomeReceiver
import androidx.annotation.RequiresApi
import androidx.credentials.provider.*
import androidx.credentials.exceptions.*
import org.json.JSONObject

@RequiresApi(34)
class ArbiterCredentialProvider : CredentialProviderService() {
    private fun action(operation: String,id: String=""): PendingIntent {
        val intent=Intent(this,CredentialProviderActivity::class.java).setAction(operation)
            .setData(Uri.parse("arbiter-credential://$operation/${Uri.encode(id)}"))
            .putExtra("com.arbiter.mobile.record",id)
        // Only the system receives this token. Its target is not exported; the
        // system fills in its authoritative provider request at selection time.
        return PendingIntent.getActivity(this,0,intent,PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
    }
    override fun onClearCredentialStateRequest(request: ProviderClearCredentialStateRequest,cancellationSignal: CancellationSignal,
        callback: OutcomeReceiver<Void?,ClearCredentialException>) { callback.onResult(null) }
    override fun onBeginGetCredentialRequest(request: BeginGetCredentialRequest,cancellationSignal: CancellationSignal,
        callback: OutcomeReceiver<BeginGetCredentialResponse,GetCredentialException>) {
        try {
            val response=BeginGetCredentialResponse.Builder()
            val origin=PlatformCredentialVault.trustedOrigin(this,request.callingAppInfo)
            if (origin!=null) for(option in request.beginGetCredentialOptions.filterIsInstance<BeginGetPublicKeyCredentialOption>()) {
                if (option.clientDataHash?.size!=32 || option.requestJson.length>131072) continue
                val options=JSONObject(option.requestJson)
                val rp=PlatformCredentialVault.relyingParty(origin,options,false)
                for(record in PlatformCredentialVault.index(this)) {
                    if (record.optString("kind")!="passkey" || record.optString("rpId")!=rp || !PlatformCredentialVault.allowed(options,record.optString("credentialId"))) continue
                    response.addCredentialEntry(PublicKeyCredentialEntry.Builder(this,record.optString("username").ifEmpty { rp },action("get",record.getString("id")),option)
                        .setDisplayName(rp).build())
                }
            }
            if (!cancellationSignal.isCanceled) callback.onResult(response.build())
        } catch (_:Exception) { callback.onError(GetCredentialUnknownException("Arbiter could not verify this credential request.")) }
    }
    override fun onBeginCreateCredentialRequest(request: BeginCreateCredentialRequest,cancellationSignal: CancellationSignal,
        callback: OutcomeReceiver<BeginCreateCredentialResponse,CreateCredentialException>) {
        try {
            val response=BeginCreateCredentialResponse.Builder()
            val origin=PlatformCredentialVault.trustedOrigin(this,request.callingAppInfo)
            if (origin!=null && request is BeginCreatePublicKeyCredentialRequest && request.clientDataHash?.size==32 && request.requestJson.length<=131072) {
                val rp=PlatformCredentialVault.relyingParty(origin,JSONObject(request.requestJson),true)
                response.setCreateEntries(listOf(CreateEntry.Builder("Arbiter",action("create")).setDescription("Save a passkey for $rp").build()))
            }
            if (!cancellationSignal.isCanceled) callback.onResult(response.build())
        } catch (_:Exception) { callback.onError(CreateCredentialUnknownException("Arbiter could not verify this credential request.")) }
    }
}
