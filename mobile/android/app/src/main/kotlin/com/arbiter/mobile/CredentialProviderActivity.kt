package com.arbiter.mobile

import android.app.Activity
import android.content.Intent
import android.hardware.biometrics.BiometricManager.Authenticators
import android.hardware.biometrics.BiometricPrompt
import android.os.Bundle
import android.os.CancellationSignal
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Button
import android.graphics.Color
import androidx.annotation.RequiresApi
import androidx.credentials.*
import androidx.credentials.provider.*
import org.json.JSONObject

@RequiresApi(34)
class CredentialProviderActivity : Activity() {
    private val cancellation=CancellationSignal()
    private var verifying=false
    private var options: JSONObject?=null
    private var rp=""
    private var clientHash: ByteArray?=null
    private var create=false
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState);window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        try {
            create=intent.action=="create"
            val origin: String
            if (create) {
                val request=PendingIntentHandler.retrieveProviderCreateCredentialRequest(intent) ?: error("No request")
                origin=PlatformCredentialVault.trustedOrigin(this,request.callingAppInfo) ?: error("Unverified caller")
                val publicKey=request.callingRequest as? CreatePublicKeyCredentialRequest ?: error("Unsupported request")
                require(publicKey.requestJson.length<=131072);options=JSONObject(publicKey.requestJson);clientHash=publicKey.clientDataHash
            } else {
                val request=PendingIntentHandler.retrieveProviderGetCredentialRequest(intent) ?: error("No request")
                origin=PlatformCredentialVault.trustedOrigin(this,request.callingAppInfo) ?: error("Unverified caller")
                val publicKey=request.credentialOptions.filterIsInstance<GetPublicKeyCredentialOption>().single()
                require(publicKey.requestJson.length<=131072);options=JSONObject(publicKey.requestJson);clientHash=publicKey.clientDataHash
            }
            require(clientHash?.size==32);rp=PlatformCredentialVault.relyingParty(origin,options!!,create)
        } catch (_:Exception) { setResult(RESULT_CANCELED);finish();return }
        val layout=LinearLayout(this).apply { orientation=LinearLayout.VERTICAL;gravity=android.view.Gravity.CENTER_VERTICAL;setPadding(64,48,64,48);setBackgroundColor(Color.rgb(16,23,32)) }
        layout.addView(TextView(this).apply {text=if(create) "Save an Arbiter passkey" else "Sign in with Arbiter";textSize=25f;setTextColor(Color.WHITE)})
        layout.addView(TextView(this).apply {text="Continue to $rp";textSize=18f;setTextColor(Color.LTGRAY);setPadding(0,24,0,32)})
        layout.addView(Button(this).apply {text="Unlock";setOnClickListener { authenticate() }})
        layout.addView(Button(this).apply {text="Cancel";setOnClickListener { setResult(RESULT_CANCELED);finish() }})
        setContentView(layout); authenticate()
    }
    private fun authenticate() {
        if(verifying || isFinishing) return;verifying=true
        val prompt=BiometricPrompt.Builder(this).setTitle("Unlock Arbiter").setSubtitle(rp)
            .setAllowedAuthenticators(Authenticators.BIOMETRIC_STRONG or Authenticators.DEVICE_CREDENTIAL).build()
        prompt.authenticate(cancellation,mainExecutor,object:BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationError(code: Int,message: CharSequence) { verifying=false;setResult(RESULT_CANCELED);finish() }
            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                PlatformCredentialVault.afterVerification(this@CredentialProviderActivity,{ key,document ->
                    try {
                        val response=Intent()
                        if(create) {
                            val result=PlatformPasskey.create(this@CredentialProviderActivity,key,document,options!!,rp,clientHash!!)
                            PendingIntentHandler.setCreateCredentialResponse(response,CreatePublicKeyCredentialResponse(result))
                        } else {
                            val id=intent.getStringExtra("com.arbiter.mobile.record") ?: error("No selected credential")
                            val result=PlatformPasskey.assertion(document,id,options!!,rp,clientHash!!)
                            PendingIntentHandler.setGetCredentialResponse(response,GetCredentialResponse(PublicKeyCredential(result)))
                        }
                        runOnUiThread { setResult(RESULT_OK,response);finish() }
                    } catch (_:Exception) { runOnUiThread { setResult(RESULT_CANCELED);finish() } }
                },{ runOnUiThread { setResult(RESULT_CANCELED);finish() } })
            }
        })
    }
    override fun onDestroy() { cancellation.cancel();super.onDestroy() }
}
