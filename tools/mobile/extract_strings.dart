import 'dart:io';
import 'dart:convert';
import 'package:analyzer/dart/analysis/utilities.dart';
import 'package:analyzer/dart/ast/ast.dart';
import 'package:analyzer/dart/ast/visitor.dart';

final messages=<String>{};
class Edits extends RecursiveAstVisitor<void> {
  final String file;
  final edits=<(int,int,String)>[];
  Edits(this.file);
  void process(String kind, AstNode name, NodeList<Expression> args) {
    if((kind=='UiText'||kind=='Text')&&args.isNotEmpty) {
      final first=args.first;
      if(first is StringInterpolation) {
        final parts=first.elements.whereType<InterpolationString>().map((e)=>e.value).join();
        if(kind=='UiText'||RegExp('[A-Za-z]{2,}').hasMatch(parts)) {
          final values=<String>[];var source='';
          for(final part in first.elements) {
            if(part is InterpolationString) source+=part.value;
            if(part is InterpolationExpression) {source+='\${${values.length}}'.replaceFirst(r'$', '');values.add('(${part.expression.toSource()}).toString()');}
          }
          messages.add(source);
          if(kind=='Text')edits.add((name.offset,name.length,'UiText'));
          edits.add((first.offset,first.length,'${jsonEncode(source)}, values:[${values.join(',')}]'));
        }
      }
      if(kind=='Text'&&first is SimpleIdentifier&&file.endsWith('onboarding.dart')&&['title','body'].contains(first.name)) {
        edits.add((name.offset,name.length,'UiText'));
      }
      if(kind=='Text'&&first is ConditionalExpression&&first.thenExpression is SimpleStringLiteral&&first.elseExpression is SimpleStringLiteral) {
        edits.add((name.offset,name.length,'UiText'));
      }
    }
  }
  @override
  void visitInstanceCreationExpression(InstanceCreationExpression node) {
    process(node.constructorName.type.name.lexeme,node.constructorName.type,node.argumentList.arguments);
    super.visitInstanceCreationExpression(node);
  }
  @override
  void visitMethodInvocation(MethodInvocation node) {
    process(node.methodName.name,node.methodName,node.argumentList.arguments);
    super.visitMethodInvocation(node);
  }
  @override
  void visitStringInterpolation(StringInterpolation node) {
    final p=node.parent;
    if(p is ArgumentList && p.arguments.isNotEmpty && identical(p.arguments.first,node)) {
      final call=p.parent;
      final name=call is MethodInvocation ? call.methodName.name : call is InstanceCreationExpression ? call.constructorName.type.name.lexeme : '';
      if(name=='Text'||name=='UiText'){super.visitStringInterpolation(node);return;}
    }
    AstNode? parent=node.parent;bool ui=false;
    while(parent!=null && parent is! Statement) {
      if(parent is MethodInvocation && ['Text','UiText','_confirm','_message'].contains(parent.methodName.name)){ui=true;break;}
      if(parent is InstanceCreationExpression && ['Text','UiText'].contains(parent.constructorName.type.name.lexeme)){ui=true;break;}
      parent=parent.parent;
    }
    final segments=node.elements.whereType<InterpolationString>().map((e)=>e.value).join();
    if(ui&&RegExp('[A-Za-z]{2,}').hasMatch(segments)) {
      var source='';final values=<String>[];
      for(final part in node.elements){if(part is InterpolationString)source+=part.value;if(part is InterpolationExpression){source+='{${values.length}}';values.add(part.expression.toSource());}}
      messages.add(source);edits.add((node.offset,node.length,'tf(${jsonEncode(source)},[${values.join(',')}])'));
    }
    super.visitStringInterpolation(node);
  }
  @override
  void visitSimpleStringLiteral(SimpleStringLiteral node) {
    final s=node.value;
    if(RegExp('[A-Za-z]').hasMatch(s)&&!s.contains(r'$')&&!s.startsWith('package:')&&!s.startsWith('dart:')&&!s.endsWith('.dart')&&!s.endsWith('.txt')&&
      (s.contains(RegExp(r'\s'))||RegExp(r'^[A-Z][a-z]+(?:[ ?!.]|$)').hasMatch(s))) messages.add(s);
    super.visitSimpleStringLiteral(node);
  }
}
void main() {
  messages.addAll((jsonDecode(File("mobile/assets/i18n/en.json").readAsStringSync()) as Map).keys.cast<String>());
  for(final file in Directory('mobile/lib').listSync().whereType<File>().where((f)=>f.path.endsWith('.dart')&&!f.path.endsWith('l10n.dart'))) {
    var source=file.readAsStringSync();
    final visitor=Edits(file.path);parseString(content:source).unit.accept(visitor);
    visitor.edits.sort((a,b)=>b.$1.compareTo(a.$1));
    for(final edit in visitor.edits) source=source.replaceRange(edit.$1,edit.$1+edit.$2,edit.$3);
    file.writeAsStringSync(source);
  }
  final sorted=messages.where((s)=>s.length<3000).toList()..sort();
  File('mobile/assets/i18n/en.json').writeAsStringSync(const JsonEncoder.withIndent('  ').convert({for(final s in sorted)s:s})+'\n');
  stdout.writeln('Extracted ${sorted.length} messages');
}
