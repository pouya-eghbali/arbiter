package com.arbiter.mobile
import android.content.Context
import org.json.JSONObject
import java.io.File
import java.util.Locale
object MobileStrings {
    private val catalogs=mutableMapOf<String,JSONObject>()
    fun language(context:Context,code:String) {
        val temporary=File(context.filesDir,"interface-language-${java.util.UUID.randomUUID()}.tmp")
        try {temporary.writeText(code);check(temporary.renameTo(File(context.filesDir,"interface-language")))}finally{temporary.delete()}
    }
    fun text(context:Context,source:String,vararg values:String):String {
        val selected=try{File(context.filesDir,"interface-language").readText()}catch(_:Exception){"system"}
        val code=if(selected=="system")Locale.getDefault().language else selected
        val supported=setOf("en","de","fr","it","es","pt","ja","zh")
        val language=if(code in supported)code else "en"
        val catalog=catalogs.getOrPut(language){try{JSONObject(context.assets.open("flutter_assets/assets/i18n/$language.json").bufferedReader().use{it.readText()})}catch(_:Exception){JSONObject()}}
        var result=catalog.optString(source,source)
        values.forEachIndexed{i,value->result=result.replace("{$i}",value)}
        return result
    }
}
